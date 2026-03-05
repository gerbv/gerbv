/* dynload.h */
/* Original Copyright (c) 1999 Alexander Shendi     */
/* Modifications for NT and dl_* interface: D. Souflis */

/** \file dynload.h
    \brief Header info for the dynamic loader functions for TinyScheme
    \ingroup gerbv
*/

#ifndef DYNLOAD_H
#define DYNLOAD_H

#include "scheme-private.h"

SCHEME_EXPORT pointer scm_load_ext(scheme *sc, pointer arglist);

#endif
