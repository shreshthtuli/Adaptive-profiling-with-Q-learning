/*===---- stddef.h - Basic type definitions --------------------------------===
 *
 * Copyright (c) 2008 Eli Friedman
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *===-----------------------------------------------------------------------===
 */

#if !defined(__STDDEF_H) || defined(__need_ptrdiff_t) ||                       \
    defined(__need_size_t) || defined(__need_wchar_t) ||                       \
    defined(__need_NULL) || defined(__need_wint_t)

#if !defined(__need_ptrdiff_t) && !defined(__need_size_t) &&                   \
    !defined(__need_wchar_t) && !defined(__need_NULL) &&                       \
    !defined(__need_wint_t)
#define __STDDEF_H
#define __need_ptrdiff_t
#define __need_size_t
#define __need_wchar_t
#define __need_NULL
/* __need_wint_t is intentionally not defined here. */
#endif

# if __LP64__
#  define __PTRDIFF_TYPE__ __int64
# else
#  define __PTRDIFF_TYPE__ __int32
# endif

#if defined(__need_ptrdiff_t)
#if !defined(_PTRDIFF_T)
#define _PTRDIFF_T
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif
#undef __need_ptrdiff_t
#endif /* defined(__need_ptrdiff_t) */

#ifndef __SIZE_TYPE__
#ifdef __i386__
# define __SIZE_TYPE__ unsigned int
#else
# define __SIZE_TYPE__ long unsigned int
#endif
#endif

#if defined(__need_size_t)
#if !defined(_SIZE_T)
#define _SIZE_T
#ifdef __i386__
// MSVC 64 bit appear to have built-in size_t
typedef __SIZE_TYPE__ size_t;
#endif
#endif
#undef __need_size_t
#endif /*defined(__need_size_t) */


#if defined(__STDDEF_H)
/* ISO9899:2011 7.20 (C11 Annex K): Define rsize_t if __STDC_WANT_LIB_EXT1__ is
 * enabled. */
#if (defined(__STDC_WANT_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__ >= 1 && \
     !defined(_RSIZE_T))
/* Always define rsize_t when modules are available. */
#define _RSIZE_T
typedef __SIZE_TYPE__ rsize_t;
#endif
#endif /* defined(__STDDEF_H) */

#ifndef __WCHAR_TYPE__
#define __WCHAR_TYPE__ unsigned short
#endif

#if defined(__need_wchar_t)
#ifndef __cplusplus
#if !defined(_WCHAR_T)
#define _WCHAR_T
#if defined(_MSC_EXTENSIONS)
#define _WCHAR_T_DEFINED
#endif
typedef __WCHAR_TYPE__ wchar_t;
#endif
#endif
#undef __need_wchar_t
#endif /* defined(__need_wchar_t) */

#if defined(__need_NULL)
#undef NULL
#ifdef __cplusplus
#  if !defined(__MINGW32__) && !defined(_MSC_VER)
#    define NULL __null
#  else
#    define NULL 0
#  endif
#else
#  define NULL ((void*)0)
#endif
#ifdef __cplusplus
#if defined(_MSC_EXTENSIONS) && defined(_NATIVE_NULLPTR_SUPPORTED)
namespace std { typedef decltype(nullptr) nullptr_t; }
using ::std::nullptr_t;
#endif
#endif
#undef __need_NULL
#endif /* defined(__need_NULL) */

#if defined(__STDDEF_H)

#if __STDC_VERSION__ >= 201112L || __cplusplus >= 201103L
#if !defined(__CLANG_MAX_ALIGN_T_DEFINED)
#ifndef _MSC_VER
typedef struct {
  long long __clang_max_align_nonce1
      __attribute__((__aligned__(__alignof__(long long))));
  long double __clang_max_align_nonce2
      __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
#else
typedef double max_align_t;
#endif
#define __CLANG_MAX_ALIGN_T_DEFINED
#endif
#endif

/* Define offsetof macro */
#ifdef __cplusplus

#ifdef  _WIN64
#define offsetof(s,m)   (size_t)( (ptrdiff_t)&reinterpret_cast<const volatile char&>((((s *)0)->m)) )
#else
#define offsetof(s,m)   (size_t)&reinterpret_cast<const volatile char&>((((s *)0)->m))
#endif

#else

#ifdef  _WIN64
#define offsetof(s,m)   (size_t)( (ptrdiff_t)&(((s *)0)->m) )
#else
#define offsetof(s,m)   (size_t)&(((s *)0)->m)
#endif

#endif	/* __cplusplus */
#endif  /* __STDDEF_H */

/* Some C libraries expect to see a wint_t here. Others (notably MinGW) will use
__WINT_TYPE__ directly; accommodate both by requiring __need_wint_t */
#if defined(__need_wint_t)
#if !defined(_WINT_T)
#define _WINT_T
typedef __WINT_TYPE__ wint_t;
#endif
#undef __need_wint_t
#endif /* __need_wint_t */


#ifdef __cplusplus
extern "C" {
#endif
int __cdecl _resetstkoflw(void);
#ifdef __cplusplus
}
#endif

#endif
