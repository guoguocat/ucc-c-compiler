#ifndef __SYS_TYPES_H
#define __SYS_TYPES_H

#ifdef __LP64__
#  define __WORDSIZE    64
#else
#  define __WORDSIZE    32
#endif

#ifdef __TYPES_WITH_MACROS
# warning types defined using macros
# define int8_t     signed char
# define uint8_t  unsigned char
# define int32_t    signed int
# define uint32_t unsigned int
# define size_t   unsigned int
# define ssize_t    signed int
# define off_t    unsigned int
#else

typedef signed   char   int8_t;
typedef unsigned char  uint8_t;
# ifdef __GOT_SHORT_LONG
typedef signed   short  int16_t;
typedef unsigned short uint16_t;
# endif

typedef signed   int    int32_t;
typedef unsigned int   uint32_t;

#ifdef __x86_64__
# ifdef __GOT_SHORT_LONG
typedef signed   long   int64_t;
typedef unsigned long  uint64_t;
#else
typedef signed   int    int64_t;
typedef unsigned int   uint64_t;
# endif
#endif


#ifdef __GOT_SHORT_LONG
typedef unsigned long  size_t; // TODO: depend on __SIZE_TYPE__ ? same for PTRDIFF_TYPE ?
typedef   signed long ssize_t;
#else
typedef unsigned int  size_t; // TODO: depend on __SIZE_TYPE__ ? same for PTRDIFF_TYPE ?
typedef   signed int ssize_t;
#endif

#ifdef __GOT_SHORT_LONG
typedef unsigned long off_t;
#else
typedef unsigned int off_t;
#endif

typedef unsigned int ino_t;
#endif

#define __offsetof(type, field) ((__typeof(sizeof(int)))(&((type *)0)->field))

#endif
