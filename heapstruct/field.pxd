# cython: language_level=3
# cython: binding=True
# cython: annotation_typing = True
cimport cython 

cdef object NODEFAULT

cdef enum FF:
    FF_Any = 0
    FF_List = 1
    FF_DICT = 2

cdef class Field:
    cdef:
        public api object name
        # C2059 does not like the term "default"
        public object default "_default" 
        public object factory
    cdef object __get_default__(self)

        
@cython.final
cdef class Factory:
    cdef:
        # ifactory means inner-factory
        object ifactory
        FF _flag