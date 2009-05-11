/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#ifndef MSPACK_SYSTEM_H
#define MSPACK_SYSTEM_H 1

/* ensure config.h is read before mspack.h */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "mspack.h"

/* fix for problem with GCC 4 and glibc (thanks to Ville Skytta)
 * http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=150429
 */
#ifdef read
# undef read
#endif

#ifdef DEBUG
# include <stdio.h>
# define D(x) do { printf("%s:%d ",__FILE__, __LINE__); \
                   printf x ; fputc('\n', stdout); fflush(stdout);} while (0);
#else
# define D(x)
#endif

/* endian-neutral reading of little-endian data */
#define __egi32(a,n) ( ((((unsigned char *) a)[n+3]) << 24) | \
		       ((((unsigned char *) a)[n+2]) << 16) | \
		       ((((unsigned char *) a)[n+1]) <<  8) | \
		       ((((unsigned char *) a)[n+0])))
#define EndGetI64(a) ((((unsigned long long int) __egi32(a,4)) << 32) | \
		      ((unsigned int) __egi32(a,0)))
#define EndGetI32(a) __egi32(a,0)
#define EndGetI16(a) ((((a)[1])<<8)|((a)[0]))

/* endian-neutral reading of big-endian data */
#define EndGetM32(a) (((((unsigned char *) a)[0]) << 24) | \
		      ((((unsigned char *) a)[1]) << 16) | \
		      ((((unsigned char *) a)[2]) <<  8) | \
		      ((((unsigned char *) a)[3])))
#define EndGetM16(a) ((((a)[0])<<8)|((a)[1]))

extern struct mspack_system *mspack_default_system;

/* returns the length of a file opened for reading */
extern int mspack_sys_filelen(struct mspack_system *system,
			      struct mspack_file *file, off_t *length);

/* validates a system structure */
extern int mspack_valid_system(struct mspack_system *sys);

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#define HAVE_MEMCMP 1

#if HAVE_MEMCMP
# define mspack_memcmp memcmp
#else
/* inline memcmp() */
static inline int mspack_memcmp(const void *s1, const void *s2, size_t n) {
  unsigned char *c1 = (unsigned char *) s1;
  unsigned char *c2 = (unsigned char *) s2;
  if (n == 0) return 0;
  while (--n && (*c1 == *c2)) c1++, c2++;
  return *c1 - *c2;
}
#endif

#endif
