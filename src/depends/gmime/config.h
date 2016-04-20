/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if cryptography in GMime is enabled. */
/* #undef ENABLE_CRYPTOGRAPHY */

/* Define if GMime should enable experimental S/MIME support. */
/* #undef ENABLE_SMIME */

/* Define if GMime should enable warning output. */
/* #undef ENABLE_WARNINGS */

/* Define to the GMime version */
#define GMIME_VERSION "2.6.20"

/* Define if libc defines an altzone variable */
/* #undef HAVE_ALTZONE */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if you have the `fsync' function. */
/* #undef HAVE_FSYNC */

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `getdomainname' function. */
/* #undef HAVE_GETDOMAINNAME */

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the <getopt.h> header with the GNU `getopt_long'
   function. */
/* #undef HAVE_GETOPT_H */

/* Define to 1 if you have the `getpagesize' function. */
/* #undef HAVE_GETPAGESIZE */

/* Define to 1 if you have the `gmtime_r' function. */
/* #undef HAVE_GMTIME_R */

/* Define to 1 if you have the `gmtime_s' function. */
#define HAVE_GMTIME_S 1

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* Define to 1 to use auto-detected iconv-friendly charset names. */
/* #undef HAVE_ICONV_DETECT_H */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #undef HAVE_INTTYPES_H */

/* Define to 1 if you have the `localtime' function. */
#define HAVE_LOCALTIME 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have a working `mmap' system call. */
/* #undef HAVE_MMAP */

/* Define to 1 if you have the `msync' function. */
/* #undef HAVE_MSYNC */

/* Define to 1 if you have the `munmap' function. */
/* #undef HAVE_MUNMAP */

/* Define to 1 if you have the <netdb.h> header file. */
/* #undef HAVE_NETDB_H */

/* Define to 1 if you have the `poll' function. */
/* #undef HAVE_POLL */

/* Define to 1 if you have the <poll.h> header file. */
/* #undef HAVE_POLL_H */

/* Define to 1 if you have the <regex.h> header file. */
/* #undef HAVE_REGEX_H */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #undef HAVE_SYS_MMAN_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if libc defines a timezone variable */
/* #undef HAVE_TIMEZONE */

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define if struct tm has a tm_gmtoff member */
/* #undef HAVE_TM_GMTOFF */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if struct utsname has a domainname member */
/* #undef HAVE_UTSNAME_DOMAINNAME */

/* Define to 1 if you have the <winsock2.h> header file. */
#define HAVE_WINSOCK2_H 1

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Define if libc defines a _timezone variable */
/* #undef HAVE__TIMEZONE */

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define with a value if your <sys/param.h> does not define MAXHOSTNAMELEN */
#define MAXHOSTNAMELEN 64

/* Define to 0 if your system does not have the O_LARGEFILE flag */
#define O_LARGEFILE 0

/* Name of package */
#define PACKAGE "gmime"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=gmime"

/* Define to the full name of this package. */
#define PACKAGE_NAME "gmime"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "gmime 2.6.20"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gmime"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.6.20"

/* The size of `off_t', as computed by sizeof. */
#define SIZEOF_OFF_T 4

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 4

/* The size of `ssize_t', as computed by sizeof. */
#define SIZEOF_SSIZE_T 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if GMime should enable stricter parsing rules. */
/* #undef STRICT_PARSER */

/* Version number of package */
#define VERSION "2.6.20"

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `int' if <sys/types.h> does not define. */
#define mode_t int

/* Define to `unsigned long int' if <poll.h> does not define. */
#define nfds_t unsigned long int

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> does not define. */
#define ssize_t int
