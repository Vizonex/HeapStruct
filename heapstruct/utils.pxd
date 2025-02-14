# cython: language_level=3
# cython: binding=True

# cimport cython
from cpython.object cimport PyObject, PyTypeObject
# from cpython.dict cimport PyDict_New
# from cpython.tuple cimport PyTuple_Check, PyTuple_GET_SIZE, PyTuple_GET_ITEM
# from cpython.ref cimport Py_INCREF, Py_DECREF
# from cpython.exc cimport PyErr_Format


cdef extern from "Python.h":
    void PyObject_GC_Track(void*)

# cdef extern from "abstract.h":
#     inline Py_ssize_t PyVectorcall_NARGS(Py_ssize_t n)


# https://cython.readthedocs.io/en/stable/src/userguide/extension_types.html#external-extension-types

# cdef extern from "object.h":    
#     ctypedef struct PyHeapTypeObject:
#         pass
    
#     ctypedef PyObject* (*vectorcallfunc)(
#         PyTypeObject* callable, const PyObject ** args, size_t nargsf, PyObject *kwnames
#     )
#     object PyObject_CallOneArg(object func, object arg)


cdef extern from "structmember.h":
    ctypedef struct PyMemberDef:
        const char* name
        int type
        Py_ssize_t offset
        int flags
        const char* doc


cdef extern from "hs_utils.h":
    bint HS_UNICODE_EQ(object, object)
    bint HS_LIKELY(bint)
    bint HS_UNLIKELY(bint)
    dict HS_GET_TYPE_DICT(object base)
    
    const char* unicode_str_and_size(object str, Py_ssize_t* size)
    const char* unicode_str_and_size_nocheck(object str, Py_ssize_t* size)







