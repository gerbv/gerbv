/*
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
 *
 *   Copyright (C) 2022 Stephen J. Hardy
 *
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/** \file x2attr.c
    \brief RS274-X2 attribute management functions
    \ingroup libgerbv
*/

#include "gerbv.h"

#include <ctype.h>

#include "common.h"
#include "gerber.h"
#include "gerb_stats.h"

#include "x2attr.h"


//#define dprintf if(DEBUG) printf
#define dprintf if(1) printf


/* Dynamic attribute dictionary state while parsing RS274-X2. 

Implementation note:

Attribute names and values are interned using g_intern_string(), which allows hanshing and equality
testing to be performed on the *pointers* rather than possibly lengthy strings.

Three of these hash tables are referenced from gerb_state_t, one for each type.  When inserting a
new name, the *other* two tables are checked to ensure uniqueness over all attribute types, as
required by the standard.


*/
typedef struct x2attr_dict {
    x2attr_type_t type;
    GHashTable * hashtable;
} x2attr_dict_t;

/* Attribute table (state) retained after parsing, which
represents a snapshot of attributes associated with image, aperture
or net objects, from the above dictionary object(s).

Library users access using x2attr_get_image_attr() etc., modify using
x2attr_set_image_attr() etc.  This is also used when importing PnP and
IPC-D-356 files.

When parsing RS274-X2, a private implementation is used to create these
according to the standard.

This is stored as a (possibly) sorted table of tuples (const char * key, const char * value).
NOTE: sorting is by the actual
key value, not the pointer which is essentially random.  The purpose of sorting is
to make search more efficient if necessary, but primarily to create a consistent
canonical attribute output order when writing RS274-X2.  This helps with testing.

We could store a hash table, but that's a fairly heavyweight structure,
so use this vector-based approach instead.  The initial link pointer
indicates the next table to search if this one comes up empty.  That's used to
point to the aperture attributes if this is a net attribute table.

This is a variable length struct, so use g_malloc.  Most of the time, its
size is known at creation time.  Otherwise, g_realloc is used to resize.
We don't bother to track allocation - it's as big as it needs to be.

Expect not that many different attribute keys, but a lot of objects and
values, so a small slow struct is better than a big fast one.

*/
typedef struct x2attr_state {
    gboolean sorted;        // TRUE when keys are sorted in canonical order.  This is
                            // only set when _sort() called.  Reset on any modification.
    unsigned size;          // Number of elements in following
    struct x2attr_state * next; // Aperture attributes if this is for net (object), etc.
    struct {
        const char * key;   // Pointer to interned UTF-8 null terminated string.
                            // Sort by key if 'sorted'.
        const char * value; // Pointer to interned UTF-8 null terminated string
    } t[1]; // 1st of 'size' elements.
} x2attr_state_t;


/******************
  Local functions on xattr_state
  
  NOTE: const char * parameters must be interned strings!  Call with g_intern_string(foo) if in doubt.
  
*******************/

static const char *
_find_value(const x2attr_state_t * s, const char * key)
{
    unsigned i;
    if (s)
        for (i = 0; i < s->size; ++i)
            if (key == s->t[i].key)
                return s->t[i].value;
    return NULL;
}

static int
_find_index(const x2attr_state_t * s, const char * key)
{
    unsigned i;
    if (s)
        for (i = 0; i < s->size; ++i)
            if (key == s->t[i].key)
                return (int)i;
    return -1;
}

// Like _find_value(), except will look through the linked list.
static const char *
_find_chained_value(const x2attr_state_t * s, const char * key)
{
    while (s) {
        const char * v = _find_value(s, key);
        if (v)
            return v;
        s = s->next;
    }
    return NULL;
}

static void
_populate(gpointer key, gpointer value, gpointer user_data)
{
    x2attr_state_t * s = (x2attr_state_t *)user_data;
    s->t[s->size].key = (const char *)key;
    s->t[s->size].value = (const char *)value;
    ++s->size;
}

static x2attr_state_t *
_new_from_hash_table(GHashTable * source)
{
    x2attr_state_t * s;
    
    if (source) {
        int size = g_hash_table_size(source);
        s = (x2attr_state_t *)g_malloc0(sizeof(x2attr_state_t) + sizeof(const char *)*2*(size-1));
        g_hash_table_foreach(source, _populate, s);
    }
    else
        s = NULL;
    return s;
}

static x2attr_state_t *
_new_from_copy(const x2attr_state_t * source)
{
    x2attr_state_t * chain;
    x2attr_state_t * n;
    
    if (source) {
        chain = source->next ? _new_from_copy(source->next) : NULL;
        n = (x2attr_state_t *)g_memdup(source, sizeof(x2attr_state_t) + sizeof(const char *)*2*(source->size-1));
        n->next = chain;
        return n;
    }
    return NULL;
}

static void
_destroy(x2attr_state_t * s)
{
    // Note that the interned string keys and values are owned by glib, so don't free here.
    if (s)
        g_free(s);
}

// Returns possibly moved state if successful, otherwise NULL if already exists and not update_ok.
// Input param s may also be null, in which case a new state is allocated with 1 entry filled.
static x2attr_state_t *
_insert(x2attr_state_t * s, const char * key, const char * value, gboolean update_ok)
{
    if (!s)
        s = (x2attr_state_t *)g_malloc0(sizeof(x2attr_state_t));    // Get that single.
    else {
        int i = _find_index(s, key);
        if (i >= 0 && !update_ok)
            return NULL;
        if (i >= 0) {
            // modify - will not change sorted state.
            s->t[i].value = value;
            return s;
        }
        // insert by realloc and append
        s = (x2attr_state_t *)g_realloc(s, sizeof(x2attr_state_t) + sizeof(const char *)*2*s->size);
    }
    s->t[s->size].key = key;
    s->t[s->size].value = value;
    ++s->size;
    s->sorted = s->size <= 1;
    return s;
}

// Delete key from s, returning original key if deleted, else NULL if not found.
// Note that we don't resize down.  Will resize only when something inserted.
// Deletion cannot change sorted state.
static const char *
_delete(x2attr_state_t * s, const char * key)
{
    int i;
    if (!s)
        return NULL;
    i = _find_index(s, key);
    if (i < 0)
        return NULL;
    if (i+1 < s->size)
        memmove(s->t + i, s->t + (i+1), sizeof(s->t[0])*(s->size-i-1));
    --s->size;
    return key;
}

static gint
_compare_keys(gconstpointer a, gconstpointer b, gpointer user_data)
{
    // a, b point to key of key,value pair.  Only interested in the key.
    // keys are guaranteed to be plain ascii.
    const char * ka = *(const char **)a;
    const char * kb = *(const char **)b;
    return strcmp(ka, kb);
}

static void
_sort(x2attr_state_t * s)
{
    if (s->sorted)
        return;
    if (s->size > 1u)
        g_qsort_with_data(s->t, s->size, sizeof(s->t[0]), _compare_keys, s);
    s->sorted = TRUE;
}

static void
_enumerate(x2attr_state_t * s, x2attr_enumeration_callback_t callback, gpointer user_data)
{
    unsigned i;
    unsigned index = 0;
    
    while (s) {
        _sort(s);
        for (i = 0; i < s->size; ++i)
            callback(index, s->t[i].key, s->t[i].value, user_data);
        s = s->next;
        ++index;
    }
}


/******************
  Local functions on x2attr_dict
*******************/

static x2attr_dict_t *
_new_dict(x2attr_type_t type)
{
    x2attr_dict_t * d = g_new0 (x2attr_dict_t, 1);
    // Use the *direct* callbacks because we're using interned strings.
    d->hashtable = g_hash_table_new(g_direct_hash, g_direct_equal);
    d->type = type;
    return d;
}

static void
_destroy_dict(x2attr_dict_t * d)
{
    if (d) {
        g_hash_table_destroy(d->hashtable);
        g_free(d);
    }
}



/******************
  Implementation interface called from parsing (gerber.c)
*******************/

x2attr_state_t *
_x2attr_new_from_dict(x2attr_dict_t * source)
{
    return _new_from_hash_table(source ? source->hashtable : NULL);
}

x2attr_state_t * 
_x2attr_chain_from_dict(x2attr_state_t * curr, x2attr_dict_t * next)
{
    if (curr) {
        curr->next = _x2attr_new_from_dict(next);
        return curr;
    }
    return _x2attr_new_from_dict(next);
        
}

x2attr_state_t * 
_x2attr_duplicate(x2attr_state_t * from)
{
    return _new_from_copy(from);
}

void 
x2attr_destroy(struct x2attr_state * s)
{
    _destroy(s);
}


void 
_x2attr_handle_T(struct gerb_state * gs, gerbv_image_t * image, x2attr_type_t type, const char * cmd, 
                    gerbv_error_list_t *error_list, long int linenum, const char * filename)
{
    /* cmd is the entire "dots" of the %T{F|A|O|D}....*% line.
       type will be
        F - add file attribute
        A - add or modify aperture attribute
        O - add or modify net (object) attribute
        D - delete net or aperture attribute (or all net+aper attributes).
        
       If error, a message will be appended to the error list.
       
       Syntax for cmd is 
         <Attribute> = <AttributeName>[,<AttributeValue>]*
         <AttributeValue> = <Field>{,<Field>}
         
         <AttributeName> = [._$a-zA-Z][._$a-zA-Z0-9]{0,126}
         <Field> = [^%*,]*
         
         Field is any character except percent, aster or comma.  May include unicode either directly using UTF-8 encoding,
         or via unicode escapes \uXXXX or \UXXXXXXXX.  THe following escapes are useful to include otherwise invalid characters:
         \u0025 = %
         \u002A = *
         \u002C = ,
         \u005C = \
         
         Names must be plain alphanumeric including dot, underscore, dollar.  An initial dot indicates a standard name.
         Everything else is user-defined, and user-defined must not start with dot.
         
         All are case sensitive.
         
         This function does *not* handle the unicode escapes.  It is up to the library user to parse the values as required.
    */
    const char * p, * k;
    int keylen, vallen, i;
    char key[X2ATTR_MAX_KEYLEN+1];
    const char * val;
    x2attr_dict_t * d, * o1, * o2;
    
    if (gs->in_parea_fill) {
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Attribute definition error at line %ld in file \"%s\": "
			   "Attributes cannot be defined or modified inside G36/G37 region"),
			linenum, filename);
        return;
    }
    
    
    if (type == X2ATTR_DELETE && !*cmd) {
        // Delete with no field: means delete all aperture and object attribs.
        _destroy_dict(gs->aperture_attrs);
        gs->aperture_attrs = _new_dict(X2ATTR_APERTURE);
        _destroy_dict(gs->object_attrs);
        gs->object_attrs = _new_dict(X2ATTR_OBJECT);
        dprintf("Deleted all attrs\n");
        return;
    }
    
    // All else partition cmd by first comma, if any, into key[,value].  Value itself may have commas, but we don't
    // do anything with them in here.
    p = strchr(cmd, ',');
    keylen = p ? p-cmd : strlen(cmd);
    vallen = p ? strlen(cmd) - (keylen+1) : 0;
    
    if (keylen < X2ATTR_MIN_KEYLEN || keylen > X2ATTR_MAX_KEYLEN) {
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Attribute definition error at line %ld in file \"%s\": "
			   "Key length %d outside bounds %d..%d"),
			linenum, filename, keylen, X2ATTR_MIN_KEYLEN, X2ATTR_MAX_KEYLEN);
        return;
    }
    
    for (i=0; i < keylen; ++i) {
        char c = key[i] = cmd[i];
        if ((i ? !isalnum(c) : !isalpha(c)) && c != '.' && c != '_' && c != '$') {
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Attribute definition error at line %ld in file \"%s\": "
			       "Invalid character ordinal %d in key at position %d out of 1..%d"),
			    linenum, filename, (int)c, i+1, keylen);
            return;
        }
    }
    key[keylen] = 0;
    
    // Intern it for following use.
    k = g_intern_string(key);
    val = g_intern_string(vallen ? p+1 : "");
    
    switch (type) {
    case X2ATTR_FILE:
        d = gs->file_attrs;
        if (!d)
            gs->file_attrs = d = _new_dict(X2ATTR_FILE);
        o1 = gs->aperture_attrs;
        o2 = gs->object_attrs;
        break;
    case X2ATTR_APERTURE:
        d = gs->aperture_attrs;
        if (!d)
            gs->aperture_attrs = d = _new_dict(X2ATTR_APERTURE);
        o1 = gs->file_attrs;
        o2 = gs->object_attrs;
        break;
    case X2ATTR_OBJECT:
        d = gs->object_attrs;
        if (!d)
            gs->object_attrs = d = _new_dict(X2ATTR_OBJECT);
        o1 = gs->aperture_attrs;
        o2 = gs->file_attrs;
        break;
    case X2ATTR_DELETE:
        if (vallen)
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_WARNING, -1,
			    _("Attribute definition error at line %ld in file \"%s\": "
			       "Attribute %s delete has value data; ignored."),
			    linenum, filename, k);
        if (gs->file_attrs && g_hash_table_contains(gs->file_attrs->hashtable, k))
		    gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			    _("Attribute definition error at line %ld in file \"%s\": "
			       "Attribute %s cannot be deleted from file attributes"),
			    linenum, filename, k);
        else {
            // Don't complain if not there.
            if (gs->aperture_attrs)
                g_hash_table_remove(gs->aperture_attrs->hashtable, k);
            if (gs->object_attrs)
                g_hash_table_remove(gs->object_attrs->hashtable, k);
            dprintf("Deleted attr %s\n", k);
        }
        return;
    default:
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Attribute definition error at line %ld in file \"%s\": "
			   "Unknown attribute type %d"),
			linenum, filename, (int)type);
        return;
    }
    
    // Check key not already in *other* tables
    if ((o1 && g_hash_table_contains(o1->hashtable, k)) || (o2 && g_hash_table_contains(o2->hashtable, k))) {
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Attribute definition error at line %ld in file \"%s\": "
			   "Attribute %s already defined as different type"),
			linenum, filename, k);
        return;
    }
    
    // If this is the file attributes, then not allowed to redefine.
    if (type == X2ATTR_FILE && g_hash_table_contains(d->hashtable, k)) {
		gerbv_stats_printf(error_list, GERBV_MESSAGE_ERROR, -1,
			_("Attribute definition error at line %ld in file \"%s\": "
			   "File attribute %s already defined"),
			linenum, filename, k);
        return;
    }
    
    // Good to go...  Casts needed because of glib.
    g_hash_table_insert(d->hashtable, (void *)k, (void *)val);
    
    dprintf("Inserted attr %s=%s in table %c\n",
            k, val, (char)type);
}

void 
_x2attr_destroy_dicts(struct gerb_state * gs)
{
    _destroy_dict(gs->file_attrs);
    _destroy_dict(gs->aperture_attrs);
    _destroy_dict(gs->object_attrs);
}

x2attr_dict_t * 
_x2attr_new_dict()
{
    return _new_dict(X2ATTR_ANY);
}


const char * 
_x2attr_value_changed(x2attr_dict_t * d, const char * key, const char * value)
{
    const char * oldval = (const char *)g_hash_table_lookup(d->hashtable, key);
    if (oldval != value)
        g_hash_table_insert(d->hashtable, (void *)key, (void *)value);
    return oldval == value ? NULL : value;
}

void 
_x2attr_destroy_dict(x2attr_dict_t * d)
{
    _destroy_dict(d);
}


typedef struct ghr_data
{
    x2attr_enumeration_callback_t callback;
    gpointer user_data;
    unsigned index;
    struct x2attr_state * attrs;
} ghr_data_t;

static gboolean
_remove_callback(gpointer key, gpointer value, gpointer user_data)
{
    ghr_data_t * data = (ghr_data_t *)user_data;
    
    // Return TRUE to delete from hashtable
    if (!_find_chained_value(data->attrs, (const char *)key)) {
        data->callback(data->index, (const char *)key, (const char *)value, data->user_data);
        return TRUE;
    }
    return FALSE;
}

void 
_x2attr_foreach_attr_missing_from_dicts(struct x2attr_state * attrs, 
                    struct x2attr_dict * d1, struct x2attr_dict * d2, 
                    x2attr_enumeration_callback_t callback, gpointer user_data)
{
    // Scan d1 and d2 (if not NULL) and for each key in the dict, where the key is missing from
    // the attrs chain, invoke the callback then delete from the dict.
    // This is mainly used when emitting RS274-X2 so that %TD...*% are output.  We could ignore this,
    // but then the output file would not have exactly the correct semantics since objects would have
    // attributes that were not present in the input file.
    // We don't generate "global" deletions (%TD*%) but if that appears in the input file it is replaced
    // with as many individual deletions which would achieve the same result.
    #if 0
    // Unfortunately, the following only works in libglib2.14, but we want to support 2.0.  So do it the
    // hard way instead :-(
    struct GHashTableIter iter;
    gpointer key, value;
    unsigned index;
    
    if (!attrs)
        return;
        
    for (index = 0; index <= 1; ++index) {
        x2attr_dict_t * d = index ? d2 : d1;
        if (d && d->hashtable) {
            g_hash_table_iter_init (&iter, d->hashtable);
            while (g_hash_table_iter_next (&iter, &key, &value)) {
                if (!_find_chained_value(attrs, (const char *)key)) {
                    callback(index, (const char *)key, (const char *)value, user_data);
                    g_hash_table_iter_remove(&iter);
                }
            }
        }
    }
    #else
    // Need to wrap given callback into data for the glib remove callback.
    ghr_data_t data;
    unsigned index;
    
    data.callback = callback;
    data.user_data = user_data;
    data.attrs = attrs;
    
    if (!attrs)
        return;
        
    for (index = 0; index <= 1; ++index) {
        data.index = index;
        x2attr_dict_t * d = index ? d2 : d1;
        if (d && d->hashtable)
            g_hash_table_foreach_remove(d->hashtable, _remove_callback, &data);
    }
    
    #endif
}

/******************
  Library API
  
  Manipulates attributes programmatically i.e. not when reading
  RS274-X2.
  
  Operate on
    image->attrs
    aperture->attrs
    net->attrs.
    
*******************/


void 
x2attr_foreach_image_attr(gerbv_image_t * image, x2attr_enumeration_callback_t callback, gpointer user_data)
{
    _enumerate(image->attrs, callback, user_data);
}

void 
x2attr_foreach_aperture_attr(gerbv_aperture_t * aperture, x2attr_enumeration_callback_t callback, gpointer user_data)
{
    _enumerate(aperture->attrs, callback, user_data);
}

void 
x2attr_foreach_net_attr(gerbv_net_t * net, x2attr_enumeration_callback_t callback, gpointer user_data)
{
    _enumerate(net->attrs, callback, user_data);
}



const char * 
x2attr_get_image_attr(const gerbv_image_t * image, const char * key)
{
    return _find_value(image->attrs, g_intern_string(key));
}

const char * 
x2attr_get_image_attr_or_default(const gerbv_image_t * image, 
                const char * key, const char * dflt)
{
    const char * v = _find_value(image->attrs, g_intern_string(key));
    return v ? v : dflt;
}

const char * 
x2attr_get_aperture_attr(const gerbv_aperture_t * aperture, const char * key)
{
    return _find_value(aperture->attrs, g_intern_string(key));
}

const char * 
x2attr_get_aperture_attr_or_default(const gerbv_aperture_t * aperture, 
                const char * key, const char * dflt)
{
    const char * v = _find_value(aperture->attrs, g_intern_string(key));
    return v ? v : dflt;
}

const char * 
x2attr_get_net_attr(const gerbv_net_t * net, const char * key)
{
    return _find_chained_value(net->attrs, g_intern_string(key));
}

const char * 
x2attr_get_net_attr_or_default(const gerbv_net_t * net, 
                const char * key, const char * dflt)
{
    const char * v = _find_chained_value(net->attrs, g_intern_string(key));
    return v ? v : dflt;
}

const char * 
x2attr_get_project_attr(const gerbv_project_t * project, const char * key)
{
    return _find_chained_value(project->attrs, g_intern_string(key));
}

const char * 
x2attr_get_project_attr_or_default(const gerbv_project_t * project, 
                const char * key, const char * dflt)
{
    const char * v = _find_chained_value(project->attrs, g_intern_string(key));
    return v ? v : dflt;
}


char * 
x2attr_get_field_or_default(int field_num, const char * value, const char * dflt)
{
    const char * p;
    if (field_num < 0)
        return NULL;
    while (field_num--) {
        p = strchr(value, ',');
        if (p)
            value = p+1;
        else
            break;
    }
    if (field_num >= 0)
        return g_strdup(dflt);
        
    p = strchr(value, ',');
    
    return g_strndup(value, p ? p-value : strlen(value));
}

char * 
x2attr_get_field_or_last(int field_num, const char * value)
{
    const char * p;
    if (field_num < 0)
        return NULL;
    while (field_num--) {
        p = strchr(value, ',');
        if (p)
            value = p+1;
        else
            break;
    }
    p = strchr(value, ',');
    
    return g_strndup(value, p ? p-value : strlen(value));
}


const char * 
x2attr_set_image_attr(gerbv_image_t * image,
                       const char * key,
                       const char * value
                       )
{
    if (!value)
        return _delete(image->attrs, g_intern_string(key));
    x2attr_state_t * s = _insert(image->attrs, g_intern_string(key), g_intern_string(value), TRUE);
    if (!s)
        return NULL;
    image->attrs = s;   // in case it got realloc'd or created
    return key;
}

const char * 
x2attr_set_aperture_attr(gerbv_aperture_t * aperture,
                       const char * key,
                       const char * value
                       )
{
    if (!value)
        return _delete(aperture->attrs, g_intern_string(key));
    x2attr_state_t * s = _insert(aperture->attrs, g_intern_string(key), g_intern_string(value), TRUE);
    if (!s)
        return NULL;
    aperture->attrs = s;   // in case it got realloc'd or created
    return key;
}

const char * 
x2attr_set_net_attr(gerbv_net_t * net,
                       const char * key,
                       const char * value
                       )
{
    if (!value)
        return _delete(net->attrs, g_intern_string(key));
    x2attr_state_t * s = _insert(net->attrs, g_intern_string(key), g_intern_string(value), TRUE);
    if (!s)
        return NULL;
    net->attrs = s;   // in case it got realloc'd or created
    return key;
}

const char * 
x2attr_set_project_attr(gerbv_project_t * project,
                       const char * key,
                       const char * value
                       )
{
    if (!value)
        return _delete(project->attrs, g_intern_string(key));
    x2attr_state_t * s = _insert(project->attrs, g_intern_string(key), g_intern_string(value), TRUE);
    if (!s)
        return NULL;
    project->attrs = s;   // in case it got realloc'd or created
    return key;
}



#define _init_ua(ua, index, val) (((char **)(ua->data))[index] = (val))

static unsigned
_getxdigits(char ** pp, unsigned n)
{
    unsigned x = 0;
    while (n-- && isxdigit(*++*pp)) {
        unsigned c = **pp;
        if (isdigit(c))
            x = x*16u + (c - '0');
        else if (islower(c))
            x = x*16u + (c - 'a' + 10u);
        else
            x = x*16u + (c - 'A' + 10u);
    }
    if ((int)n < 0) 
        ++*pp;
    return x;
}

static char *
_file_to_utf8_inplace(char * s)
{
    // Do the unescaping work.  s is assumed to be an attribute field value.  Returns s.
    // Unescaping always results in length reduction, so do it in-place in the existing buffer.
    // The RS274-X2 standard does not completely describe the use of backslash as an escape character.
    // Here, we implement the following:
    //  \uXXXX  - 16 bit unicode, convert to UTF-8
    //  \UXXXXXXXX - 32 bit unicode, convert to UTF-8
    //  \\ - literal backslash
    //  \r, \n, \t - C escapes for basic whitespace
    //  \xXX - C escape for hex encoded.  Treated equivalently to \u00XX.
    //  \<other> - Probably an error, but seems harmless enough to leave them untranslated e.g. \i will remain as "\i".
    // For codes that would result in a byte ordinal value of 0x00 (null), this is permissible but leaves
    // the string truncated at that point.  It's really an error by whoever generated the file.
    // If s has UTF-8 errors, they will remain in the return.
    // We're forgiving about the number of hex digits following \u, \U, or \x: if ends before finding enough hex digits,
    // then pad on left with zeros.  If this results in a nul, then so be it.
    
    // q looks for next backslash.  r is move dest, p is move source.
    char * q, * r = s, * p = s;
    unsigned hex;
    int l;
    
    do {
        q = strchr(p, '\\');
        if (!q) {
            if (r != p)
                memmove(r, p, strlen(p)+1);
            break;
        }
        l = q - p;
        if (l && r != p)
            memmove(r, p, l);
        r += l;
        p += l;
        // Now, p and q point to a backslash; decide how to fill *r.
        ++q;
        switch (*q) {
        default:
            // Not recognized escape, copy the backslash and continue
            *r++ = *p++;
            break;
        case 'u':
            hex = _getxdigits(&q, 4);
            goto _common_hex;
        case 'U':
            hex = _getxdigits(&q, 8);
            goto _common_hex;
        case 'X':
        case 'x':
            hex = _getxdigits(&q, 2);
        _common_hex:
            if (!hex) {
                *r = 0;
                // Early return since null terminated now.
                return s;
            }
            l = g_unichar_to_utf8(hex, r);
            r += l;
            p = q;
            break;
        case 'r': *r++ = '\r'; p = q+1; break;
        case 'n': *r++ = '\n'; p = q+1; break;
        case 't': *r++ = '\t'; p = q+1; break;
        }
    } while (TRUE);
                
    
    return s;
}

static unsigned
_utf8_to_file(const char * utf8, char * outbuf)
{
    // Implementation note: since escaping the string can expand it by up to 6 times (e.g. 3 escape chars would
    // expand to \u001B\u001B\u001B), and since 99.99% of strings should be quite short, it makes sense to 
    // first count how many bytes are required before allocating the string.  Counting is much faster than
    // (re)allocating, until the string gets quite long.  Of course, you can avoid even this overhead and
    // just pass a buffer that is almost always 6 times bigger than needed.
    // Note that UTF-8 is output unchanged, so we don't need to worry about chars >= 0x80.  We only need
    // to escape control chars, and a few ascii chars.
    //
    // If this function called with outbuf NULL, return the computed buffer length required to hold the result.
    // Otherwise, outbuf assumed to be large enough for the result (plus a null term) and it is filled.
    unsigned l = 0;
    
    while (*utf8) {
        if (*utf8 < ' ' || *utf8 == 0x7F || 
            *utf8 == '%' || *utf8 == '*' || *utf8 == ',' || *utf8 == '\\') {
            l += 6;
            if (outbuf) {
                sprintf(outbuf, "\\u%04X", (unsigned)*utf8);
                outbuf += 6;
            }
        }
        else {
            ++l;
            if (outbuf)
                *outbuf++ = *utf8;
        }
        ++utf8;
    }
    if (outbuf)
        *outbuf = 0;
    return l;
}


char * 
x2attr_utf8_to_file(const char * utf8)
{
    // Return new string with arbitrary characters in 'utf8' escaped using \uXXXX convention for control chars,
    // percent, comma, backslash or asterisk.  This is so the attribute is safe to write to an RS274-X2
    // file.  Returns new string owned by caller, who is responsible for freeing it.
    // Note that Unicode chars above \u0080 are encoded with all bytes 0x80 and above, so do not need to
    // be escaped.
    unsigned l;
    char * s;
    g_assert(utf8);
    
    l = _utf8_to_file(utf8, NULL);
    s = (char *)g_malloc(l+1);
    _utf8_to_file(utf8, s);
    return s;
}

char *
x2attr_utf8_array_to_file(const GArray * utf8_array)
{
    // Return new string suitable for use as an RS274-X2 attribute value.  Each of the UTF-8 strings in the
    // array parameter is encoded using x2attr_utf8_to_file(), and they are concatenated with ',' (comma)
    // delimiters.  Other than memory constraints, there is no limit to the output string length.
    // [Technically, RS274-X files do not need any CR/LF line endings, however that would probably break
    // the existing reader.  Attributes will not have any CRLF or other control characters directly, but
    // they may be encoded using \uXXXX escapes.]
    // Note that "raw" attribute values, as returned by x2attr_get_net_attr() etc., are stored in this
    // file format, not as arrays or general C-style strings.
    // If the array has zero size, an empty string is returned.
    unsigned l, i;
    char * s, * p;
    g_assert(utf8_array);
    
    if (!utf8_array->len)
        return g_strdup("");
    
    l = 0;
    for (i = 0; i < utf8_array->len; ++i)
        l += _utf8_to_file(((char **)utf8_array->data)[i], NULL);
    l += utf8_array->len;   // for the commas and null term
    s = (char *)g_malloc(l);
    p = s;
    for (i = 0; i < utf8_array->len; ++i) {
        l = _utf8_to_file(((char **)utf8_array->data)[i], p);
        p += l;
        *p++ = i+i < utf8_array->len ? ',' : 0;
    }
    
    return s;
}

char *
x2attr_file_to_utf8(const char * attrstr, int index)
{
    // Return new string with any \uXXXX escape sequences converted to UTF-8.  This is used to convert
    // attributes, as stored in RS274-X2 files, into human readable strings.  Since attribute value
    // strings may be a comma-delimited list, the index parameter selects that field of the string.
    // Returns new string owned by caller, who is responsible for freeing it, or NULL if attrstr is NULL
    // or index is negative.  If index is positive, but does not reference a field, then the empty
    // string (not NULL) is returned.
    const char * q, * p = attrstr;
    
    if (!p || index < 0)
        return NULL;
        
    while (index-- && (q = strchr(p, ',')))
        p = q + 1;
    
    if (index < 0) {
        // Found the indexed entry first, so extract and unescape up to next comma.
        q = strchr(p, ',');
        if (!q)
            q = p + strlen(p);
        return _file_to_utf8_inplace(g_strndup(p, q-p));
    }
    
    // Reached end of fields, return empty string.
    return g_strdup("");
}

GArray *
x2attr_file_to_utf8_array(const char * attrstr)
{
    // Return new array, populated by extracting all fields from attrstr, as per x2attr_file_to_utf8().
    // The string data in the array is also owned by the caller.  For convenience, the array and its
    // data may be freed using x2attr_destroy_utf8_array().
    // There will be 0 elements in the array if attrstr is NULL.  Otherwise, the array size will always 
    // equal the number of commas in attrstr, plus 1.  Hence, if attrstr is empty, then one empty
    // string is returned in the array.  If attrstr ends wil a comma, the last element in array will
    // be an empty string.
    int i, nf = x2attr_get_num_fields(attrstr);
    const char * q, * p = attrstr;
    GArray * a = g_array_sized_new(FALSE, FALSE, sizeof(char *), nf);
    g_assert(a);
    
    a->len = nf;
    for (i = 0; i < nf; ++i) {
        q = strchr(p, ',');
        if (!q) {
            _init_ua(a, i, g_strdup(""));
            break;
        }
        _init_ua(a, i, _file_to_utf8_inplace(g_strndup(p, q-p)));
        p = q+1;
    }
    return a;
}

int
x2attr_get_num_fields(const char * attrstr)
{
    // Return the number of elements that would be returned by x2attr_file_to_utf8_array().
    // In other words, return the number of commas in attrstr, plus 1, or return 0 if
    // attrstr is NULL.
    int nf = 1;
    const char * p = attrstr;
    
    if (!p)
        return 0;
    while ((p = strchr(p, ',')))
        ++nf, ++p;
    return nf;
}

void
x2attr_destroy_utf8_array(GArray * utf8_array)
{
    // Free the given array and all its string contents.  It is assumed to be an array returned by
    // x2attr_file_to_utf8_array().
    unsigned i;
    
    if (!utf8_array)
        return;
    for (i = 0; i < utf8_array->len; ++i)
        g_free(((char **)utf8_array->data)[i]);
    g_array_free(utf8_array, TRUE);
}

