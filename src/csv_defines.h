#ifndef DEFINES_H
#define DEFINES_H
/** 	\file csv_defines.h
	\brief Sets up internal definitions for handling csv-style files
	\ingroup libgerbv
*/

#if defined(__sparc__)

#define HAVE_ENCDEC 0
#define HAVE_STRDUP 1
#define HAVE_STRNLEN 0
#define HAVE_EXPAT 0
#define HAVE_MBSTATE 0
#define HAVE_WCWIDTH 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_VARMACRO 1
#define HAVE_LANGINFO 1

#elif defined(_WIN32)

#define HAVE_ENCDEC 0
#define HAVE_STRDUP 1
#define HAVE_STRNLEN 0
#define HAVE_EXPAT 0
#define HAVE_MBSTATE 0
#define HAVE_WCWIDTH 0
#define HAVE_SNPRINTF 0
#define HAVE_VSNPRINTF 0
#define HAVE_VARMACRO 0
#define HAVE_LANGINFO 0

#else

#define HAVE_ENCDEC 0
#define HAVE_STRDUP 1
#define HAVE_STRNLEN 1
#define HAVE_EXPAT 0
#define HAVE_MBSTATE 1
#define HAVE_WCWIDTH 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 0
#define HAVE_VARMACRO 1
#define HAVE_LANGINFO 1

#endif

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#endif /* DEFINES_H */
