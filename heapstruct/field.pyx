# cython: language_level=3 
# cython: annotation_typing = True
cimport cython
from cpython.list cimport PyList_New, PyList_Check
from cpython.dict cimport PyDict_New, PyDict_Check


cdef extern from "Python.h":
    object PyObject_CallNoArgs(object func)



# Make sure NODEFAULT is given Public Access...
NODEFUALT = object()

@cython.final
cdef class Factory:

    def __cinit__(self, object ifactory):
        self.ifactory = ifactory

        if PyList_Check(ifactory):
            self._flag = FF_List
        elif PyDict_Check(ifactory):
            self._flag = FF_DICT
        else:
            self._flag = FF_Any

    def __call__(self, *args, **kwds) -> object:
        if self._flag == FF_List:
            return PyList_New(0)
        elif self._flag == FF_DICT:
            return PyDict_New()
        else:
            return PyObject_CallNoArgs(self.ifactory)
 

cdef class Field:
    
    def __cinit__(self, object default = NODEFAULT, factory= NODEFAULT, object name = None):
        if default != NODEFAULT and factory != NODEFAULT:
            raise TypeError("default and factory can't be defined at the same time")
        
        if (name is not None) and (not isinstance(name, str)):
           raise TypeError("name must be a str")
        
        if factory is not None:
            if not callable(factory):
                raise TypeError("factory but be callable")
            self.factory = Factory(factory)
        else:
            self.factory = factory

        self.name = name
        self.default = default

   
    cdef object __get_default__(self):
        if self.default != NODEFAULT:
            return self.default
        elif self.factory != NODEFAULT:
            return self.factory
        else:
            return NODEFAULT
    
    

