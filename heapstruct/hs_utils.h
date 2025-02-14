/* The Macros were copied from the msgsepc library and renamed to match our needs...

Copyright (c) 2021, Jim Crist-Harif
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
  may be used to endorse or promote products derived from this software
  without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef __HS_UTILS_H__
#define __HS_UTILS_H__

#include "Python.h"

/** Macros were copied from Msgspec into our code so we can benefit from it's optimizations */

#ifdef __GNUC__
#define HS_LIKELY(pred) __builtin_expect(!!(pred), 1)
#define HS_UNLIKELY(pred) __builtin_expect(!!(pred), 0)
#else
#define HS_LIKELY(pred) (pred)
#define HS_UNLIKELY(pred) (pred)
#endif

#ifdef __GNUC__
#define HS_INLINE __attribute__((always_inline)) inline
#define HS_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define HS_INLINE __forceinline
#define HS_NOINLINE __declspec(noinline)
#else
#define HS_INLINE inline
#define HS_NOINLINE
#endif

#define PY312_PLUS (PY_VERSION_HEX >= 0x030c0000)
#define PY313_PLUS (PY_VERSION_HEX >= 0x030d0000)

#if PY312_PLUS
#define HS_GET_TYPE_DICT(a) PyType_GetDict(a)
#else
#define HS_GET_TYPE_DICT(a) (((PyTypeObject*)(a))->tp_dict)
#endif


#if PY313_PLUS
#define HS_UNICODE_EQ(a, b) (PyUnicode_Compare(a, b) == 0)
#else
#define HS_UNICODE_EQ(a, b) _PyUnicode_EQ(a, b)
#endif


/* XXX: Optimized `PyUnicode_AsUTF8AndSize` for strs that we know have
 * a cached unicode representation. */
static inline const char *
unicode_str_and_size_nocheck(PyObject *str, Py_ssize_t *size) {
    if (HS_LIKELY(PyUnicode_IS_COMPACT_ASCII(str))) {
        *size = ((PyASCIIObject *)str)->length;
        return (char *)(((PyASCIIObject *)str) + 1);
    }
    *size = ((PyCompactUnicodeObject *)str)->utf8_length;
    return ((PyCompactUnicodeObject *)str)->utf8;
}

/* XXX: Optimized `PyUnicode_AsUTF8AndSize` */
static inline const char *
unicode_str_and_size(PyObject *str, Py_ssize_t *size) {
    const char *out = unicode_str_and_size_nocheck(str, size);
    if (HS_LIKELY(out != NULL)) return out;
    return PyUnicode_AsUTF8AndSize(str, size);
}




#endif // __HS_UTILS_H__