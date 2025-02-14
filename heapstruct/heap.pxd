# cython: language_level = 3
# cython: annotation_typing = True
cimport cython

from .utils cimport PyMemberDef
# from .heap_api cimport HeapObject
from cpython.tuple cimport PyTuple_GET_SIZE, PyTuple_GET_ITEM

# cdef extern from "object.h":
#     ctypedef struct PyHeapTypeObject:
#         pass 




cdef extern from *:
    """
// heap.pxd

#define HeapStruct_GET_MEMBERS(htype) \
    ((PyMemberDef *)(((char *)(((PyHeapTypeObject*)htype))) + Py_TYPE(htype)->tp_basicsize))

#define GET_AT_OFFSET(obj, index) ((char*)obj) + index
#define ADDR_TO_VALUE(addr) *(PyObject**)addr
#define ADDR_IS_NULL(addr) *(PyObject**)addr == NULL
#define SET_ADDR(addr, value) *(PyObject**)addr = value


    """


    PyMemberDef* HeapStruct_GET_MEMBERS(object htype)
    char* HeapStruct_GET_ADDR(object cls, object obj, Py_ssize_t index)
    object ADDR_TO_VALUE(char* addr)
    bint ADDR_IS_NULL(char* addr)
    char* GET_AT_OFFSET(object obj, Py_ssize_t index)
    void SET_ADDR(char* addr, object value)

# cdef extern from "heapapi.h":
#     cdef struct _heapobject:
#         pass

#     ctypedef _heapobject _PyHeapObject

#     ctypedef class heapstruct.heap_api.HeapObject [object _PyHeapObject, check_size ignore]:
#         cdef:
#             Py_ssize_t* s_offsets



cdef class HeapStructMixin:
    pass

# @cython.internal
cdef class _HeapStruct(type):
    cdef:
        Py_ssize_t* s_offsets
        public tuple s_fields
        public tuple s_defaults
        object post_init
        object post_new

        
        
        
    cdef inline char* __get_address__(self, Py_ssize_t index)
    cdef inline void __set_address__(self, Py_ssize_t index, object value)
    cdef inline Py_ssize_t __find_field_location__(self, unicode name)
         
    
    cdef inline Py_ssize_t nfields(self):
        return PyTuple_GET_SIZE(self.s_fields)
    
    
    cdef inline Py_ssize_t ndefaults(self):
        return PyTuple_GET_SIZE(self.s_defaults)
     
    
    cdef inline PyMemberDef* __class_memebers__(self):
        return HeapStruct_GET_MEMBERS(self)



@cython.final
cdef class InternalInfo:
    cdef:
        dict namespaces
        dict offsets_lk, defaults_lk
        list slots
        object fields, defaults, match_args

    cdef inline int set_offset_member(self, PyMemberDef* member)
    cdef inline int set_default(self, object key, object value)
    cdef inline int __collect_base__(self, object base) except -1
    
    cdef inline bint is_classvar(self, object type)
    cdef int __process_default__(self, object name, _HeapStruct base) except -1
    cdef int __process_defaults__(self, _HeapStruct base) except -1
    cdef int __create_fields__(self, _HeapStruct base) except -1
    # cdef int __finalize_base__(self, _HeapStruct base) except -1
    cdef Py_ssize_t* __construct_offsets__(self, _HeapStruct base) except NULL




