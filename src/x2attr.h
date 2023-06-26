/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
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

/** \file x2attr.h
    \brief Header info for the RS274-X2 attribute management functions
    \ingroup libgerbv
    
    Attributes are essentially a key string and associated value string.
    There are 3 types defined in the standard: File, Aperture and Object,
    however all types share the same key space.
    
    Keys are limited length strings (1-127 ascii characters).  They must be
    alphanumeric plus the characters '.', '$' or '_'.  First character
    must not be a digit.  If you write a key that begins with a non-standard
    character, the key will remain "private" and not written on export.
    For such private keys, you can basically ignore the following escape/
    unescape convention, and just store arbitrary C strings in the value,
    since then there is no need for standard compatibility.  Most of the time,
    you can process the values in native file format, since escapes
    are rarely used.  This has the advantage of much greater efficiency,
    which is why this library does not automatically translate when files
    are read and written.  For the rare cases where you need to be
    concerned, see the following.
    
    Values are arbitrary unicode, encoded using UTF-8 in the gerber file.
    To support such arbitrariness, when saved in the Gerber file,
    the characters '%', '*' and '\' (plus control characters) must
    be escaped using \uXXXX.
    
    Values may be divided into fields.  Fields are delimited by commas
    when written to the Gerber file.  Thus, if a field contains a comma
    character, each such must be escaped using \u002C to avoid confusing it
    with multiple fields when read back.
    
    When using this API, the internal structures (gerbv_image_t,
    gerbv_aperture_t, gerbv_net_t etc.) which store attributes store them
    in external file representation form.
    
    When using attributes programmatically, convert the values into normal
    C strings (and arrays of strings) using the x2attr_file_to_utf8_array()
    and related functions.
    
    Similarly, when setting attribute values to be written out to an exported
    Gerber file, first escape the C strings properly using
    x2attr_utf8_array_to_file() and related functions.
    
    The following is an example of how to obtain a file attribute, modify
    the value, then write it back ready for export.
    
    ~~~~~~~~~~~~~~~{.c}
    gerbv_image_t * image = ...;
    
    const char * val = x2attr_get_image_attr(image, "MyKey");
    char * cval = x2attr_file_to_utf8(val, 0);
    char * newcval = g_strconcat(cval, "\nfoo,bar", NULL); 
    char * newval = x2attr_utf8_to_file(newcval);
    x2attr_set_image_attr(image, "MyKey", newval);
    g_free(newval);
    g_free(newcval);
    g_free(cval);
    ~~~~~~~~~~~~~~~
    
    In the above, the attribute would be written to the gerber file like
    
    <pre>%TFMyKey,OldVal\u000Afoo\u002Cbar*%</pre>
*/

#ifndef X2ATTR_H
#define X2ATTR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

#define X2ATTR_MIN_KEYLEN   1       //!< Minimum attribute name byte length
#define X2ATTR_MAX_KEYLEN   127     //!< Maximum attribute name byte length

#define X2ATTR_MAX_VALLEN   512     //!< Maximum attribute value byte length

typedef enum x2attr_type {
	X2ATTR_ANY = 0,         // Used in cases where we want to look up in any of the following.
	X2ATTR_FILE = 'F',      // %TF*% - file scope, associated with gerbv_image_t
	X2ATTR_APERTURE = 'A',  // %TA*% - aperture scope, associated with gerbv_aperture_t, and also 
	                            // gerbv_net_t for G36/37 regions, which are deemed to be "flash 
	                            // and forget".
	X2ATTR_OBJECT = 'O',    // %TO*% - object scope, associated with gerbv_net_t
	X2ATTR_DELETE = 'D',    // %TD*% - delete by name (of all except file attributes)
} x2attr_type_t;


// Some functions visible for implementation of reading/writing attribute
// commands in an RS274-X2 file...  Not part of the library API.

struct x2attr_state * _x2attr_new_from_dict(struct x2attr_dict * source);

struct x2attr_state * _x2attr_chain_from_dict(struct x2attr_state * curr, struct x2attr_dict * next);

struct x2attr_state * _x2attr_duplicate(struct x2attr_state * from);

void x2attr_destroy(struct x2attr_state * s);

void _x2attr_handle_T(struct gerb_state * gs, gerbv_image_t * image, x2attr_type_t type, const char * cmd, 
                        gerbv_error_list_t *error_list, long int linenum, const char * filename); 

void _x2attr_destroy_dicts(struct gerb_state * gs);

// For writing out attributes only if value changed.  Requires interned strings e.g. from foreach callbacks.
struct x2attr_dict * _x2attr_new_dict();
const char * _x2attr_value_changed(struct x2attr_dict * d, const char * key, const char * value);
void _x2attr_destroy_dict(struct x2attr_dict * d);

// Externally visible API...

//! Function signature for 'foreach' callback
typedef void (*x2attr_enumeration_callback_t)(
    unsigned index, /*!< Ordinal of the chained attribute set in which this key belongs.
                         This is normally 0, except in the case of G36/37 regions, where
                         0 will indicate an object attribute, and 1 will indicate a aperture
                         attribute.  This will be important when writing out.
                     */
    const char * key, //!< key.  Will be provided in collated order within each index.
    const char * value, //!< value.  Never NULL, but may be empty string.
    gpointer user_data //!< data passed from the foreach function call.
    );

// Internal function used for emitting deletions (%TD) when attributes in tracking dictionary (or alternate)
// but not in the given attribute state.  The callback is invoked for these keys, then removed from the dict.
void _x2attr_foreach_attr_missing_from_dicts(struct x2attr_state * attrs, 
                    struct x2attr_dict * d1, struct x2attr_dict * d2, 
                    x2attr_enumeration_callback_t callback, gpointer user_data);

//! Callback specified function for each key+value file attribute pair.
void x2attr_foreach_image_attr(gerbv_image_t * image, x2attr_enumeration_callback_t callback, gpointer user_data);

//! Callback specified function for each key+value file attribute pair.
void x2attr_foreach_aperture_attr(gerbv_aperture_t * aperture, x2attr_enumeration_callback_t callback, gpointer user_data);

//! Callback specified function for each key+value file attribute pair.
void x2attr_foreach_net_attr(gerbv_net_t * net, x2attr_enumeration_callback_t callback, gpointer user_data);

//! Return file attribute value for given name, or NULL if not found.
const char * x2attr_get_image_attr(const gerbv_image_t * image, const char * key);

//! Return file attribute value for given name, or given default if not found.
const char * x2attr_get_image_attr_or_default(const gerbv_image_t * image, 
                const char * key, const char * dflt);

//! Return aperture attribute value for given name, or NULL if not found.
const char * x2attr_get_aperture_attr(const gerbv_aperture_t * aperture, const char * key);

//! Return aperture attribute value for given name, or given default if not found.
const char * x2attr_get_aperture_attr_or_default(const gerbv_aperture_t * aperture, 
                const char * key, const char * dflt);

//! Return net or aperture attribute value for given name, or NULL if not found.
const char * x2attr_get_net_attr(const gerbv_net_t * net, const char * key);

//! Return net or aperture attribute value for given name, or given default if not found.
const char * x2attr_get_net_attr_or_default(const gerbv_net_t * net, 
                const char * key, const char * dflt);
                
//! Return project attribute value for given name, or NULL if not found.
const char * x2attr_get_project_attr(const gerbv_project_t * project, const char * key);

//! Return project attribute value for given name, or given default if not found.
const char * x2attr_get_project_attr_or_default(const gerbv_project_t * project, 
                const char * key, const char * dflt);


/*! Return new string which is the specified field in the given string value.
Fields are delimited by commas.  field_num is zero-based.  If not that many fields,
return the specified default value.  If field_num negative, returns NULL.

Caller owns returned string and must g_free() it.
*/
char * x2attr_get_field_or_default(int field_num, const char * value, const char * dflt);

/*! Return new string which is the specified field in the given string value.
Fields are delimited by commas.  field_num is zero-based.  If not that many fields,
return the last or only field in the value.  If field_num negative, returns NULL.

Caller owns returned string and must g_free() it.
*/
char * x2attr_get_field_or_last(int field_num, const char * value);

/*! Add or delete a file attribute.

The standard also mandates that attribute names are unique across all types.
This is not enforced by this function.  WARNING: if the standard is violated, then 
writing out as RS274-X2 will generate non-conforming Gerber.

A value of NULL means "delete", but a value which is a zero-length string means "key present
with no value".

If the key's first char is not alphabetic ascii, or a dot, underscore or dollar sign, then the
attribute will not be exported to RS274-X2, but this is a recommended way of adding "private"
attributes.

\return the interned string of the key if successful, else NULL.
*/
const char * x2attr_set_image_attr(gerbv_image_t * image, //!< Image whose file attribute table is to be modified
                                   const char * key,    //!< File attribute name
                                   const char * value   //!< Value string, or NULL to delete
                                   );

/*! Add or delete an aperture attribute.

The standard mandates that attribute names are unique across all types.
This is not enforced by this function.  WARNING: if the standard is violated, then 
writing out as RS274-X2 will generate non-conforming Gerber.

A value of NULL means "delete", but a value which is a zero-length string means "key present
with no value".

If the key's first char is not alphabetic ascii, or a dot, underscore or dollar sign, then the
attribute will not be exported to RS274-X2, but this is a recommended way of adding "private"
attributes.

\return the interned string of the key.
*/
const char * x2attr_set_aperture_attr(gerbv_aperture_t * aperture, //!< Aperture whose file attribute table is to be modified
                                   const char * key,    //!< Aperture attribute name
                                   const char * value   //!< Value string, or NULL to delete
                                   );

/*! Add or delete a net (object) attribute.

The standard mandates that attribute names are unique across all types.
This is not enforced by this function.  WARNING: if the standard is violated, then 
writing out as RS274-X2 will generate non-conforming Gerber.

A value of NULL means "delete", but a value which is a zero-length string means "key present
with no value".

If the key's first char is not alphabetic ascii, or a dot, underscore or dollar sign, then the
attribute will not be exported to RS274-X2, but this is a recommended way of adding "private"
attributes.

\return the interned string of the key.
*/
const char * x2attr_set_net_attr(gerbv_net_t * net, //!< Net whose file attribute table is to be modified
                                   const char * key,    //!< Net attribute name
                                   const char * value   //!< Value string, or NULL to delete
                                   );

/*! Add or delete project attribute.

This is not part of the RS274-X2 standard, but is a convenient way to pass arbitrary
file processing options from the command line.  See also x2attr_get_project_attr().

\return the interned string of the key.
*/
const char * x2attr_set_project_attr(gerbv_project_t * project, //!< Project whose table is to be modified
                                   const char * key,    //!< Attribute name
                                   const char * value   //!< Value string, or NULL to delete
                                   );


//! Convert arbitrary UTF-8 encoded C string to RS274-X2 attribute value encoding.
char * x2attr_utf8_to_file(const char * utf8);

//! Convert array of arbitrary UTF-8 encoded C string to RS274-X2 attribute value encoding.
char * x2attr_utf8_array_to_file(const GArray * utf8_array);

//! Convert RS274-X2 attribute value encoding (e.g. as returned by x2attr_get_net_attr())
//! to a UTF-8 encoded C string.
char * x2attr_file_to_utf8(const char * attrstr, int index);

//! Convert comma delimited RS274-X2 attribute value encoding (e.g. as returned by x2attr_get_net_attr())
//! to an array of UTF-8 encoded C strings.  Return value should be feed using x2attr_destroy_utf8_array().
GArray * x2attr_file_to_utf8_array(const char * attrstr);

//! Return the number of (comma delimited) fields in an RS274-X2 attribute value.
int x2attr_get_num_fields(const char * attrstr);

//! Destroy the array returned by x2attr_file_to_utf8_array().
void x2attr_destroy_utf8_array(GArray * utf8_array);

#ifdef __cplusplus
}
#endif

#endif /* X2ATTR_H */
