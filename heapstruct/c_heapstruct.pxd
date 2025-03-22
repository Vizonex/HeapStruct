# cython: language_level = 3, binding=True
from cpython.object cimport Py_TYPE, PyTypeObject

cdef extern from "structmember.h":
    ctypedef struct PyMemberDef:
        const char *name
        int type
        Py_ssize_t offset
        int flags
        const char *doc


cdef extern from "heapstruct.h":
    """
/* == HEAPSTRUCT.h == */
    """

    
    Py_ssize_t HeapStructMeta_GET_NFIELDS(PyTypeObject* t)

    ctypedef struct PyHeapStructMeta:
        pass 

    PyMemberDef* HeapStruct_GET_MEMBERS(object m)

    object HeapStruct_GET_FIRST_SLOT(object m)

    # Guess here's the real question, will heapstruct._heapstruct import correctly?
    ctypedef class _heapstruct.HeapStructMeta[object PyHeapStructMeta]:
        cdef:
            # Adhere to what has already been written
            object __struct_fields__ "struct_fields"
            object __struct_defaults__ "struct_defaults"
            Py_ssize_t *struct_offsets

        @property
        cdef inline Py_ssize_t nfields(self) noexcept:
            return HeapStructMeta_GET_NFIELDS(Py_TYPE(self))

        @property
        cdef inline PyMemberDef* members(self) noexcept:
            return HeapStruct_GET_MEMBERS(self)


