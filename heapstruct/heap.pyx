# cython:language_level=3str
from cpython.dict cimport PyDict_New, PyDict_SetItemString, PyDict_GetItem, PyDict_GetItemString, PyDict_SetItem
from cpython.exc cimport PyErr_SetString, PyErr_Format, PyErr_NoMemory
from cpython.long cimport PyLong_AsSsize_t, PyLong_FromSsize_t
from cpython.list cimport PyList_New, PyList_Append, PyList_AsTuple, PyList_Sort
from cpython.object cimport Py_SIZE, PyObject
from cpython.mem cimport PyMem_Malloc, PyMem_Free
from cpython.set cimport PySet_New, PySet_GET_SIZE, PySet_Contains, PySet_Clear
from cpython.type cimport PyType_Check
from cpython.tuple cimport PyTuple_GET_SIZE, PyTuple_GET_ITEM, PyTuple_New, PyTuple_SET_ITEM
from cpython.unicode cimport PyUnicode_Check

from libc.string cimport memcmp

from .utils cimport PyMemberDef, HS_UNICODE_EQ, HS_GET_TYPE_DICT, unicode_str_and_size
from .field cimport Field, Factory, NODEFAULT

cimport cython

from typing import ClassVar, _GenericAlias

# Chache Varibale incase something were to happen such as User-Monkey-Patching
cdef ClassVarType = ClassVar 
cdef GenericAliasType = _GenericAlias



cdef class HeapStructMixin:
    pass




# @cython.internal
cdef class _HeapStruct(type):

    # @property
    # cdef inline Py_ssize_t nfields(self):
    #     return PyTuple_GET_SIZE(self.fields)

    # # @property
    # cdef inline Py_ssize_t ndefaults(self):
    #     return PyTuple_GET_SIZE(self.defaults)
    
    cdef inline char* __get_address__(self, Py_ssize_t index):
        return GET_AT_OFFSET(self, index)

    cdef inline void __set_address__(self, Py_ssize_t index, object value):
        cdef char* address = self.__get_address__(index)
        SET_ADDR(address, value)

    cdef inline Py_ssize_t __find_field_location__(self, unicode name):
        cdef Py_ssize_t i
        for i, field in enumerate(self.s_fields):
            if HS_UNICODE_EQ(field, name):
                return i
        return -1
    
    def __dealloc__(self):
        if self.s_offsets != NULL:
            PyMem_Free(self.s_offsets)


    

    
    # One of the few overridable values for making Subclassed Fields with...
    # cpdef object __create_field__(self, object name, object default = NODEFAULT, factory = NODEFAULT):
    #     return Field.__new__(Field, default=default, factory=factory, name=name)
    

@cython.final
cdef class InternalInfo:
    

    def __cinit__(self, dict namespaces = {}):
        self.offsets_lk = PyDict_New()
        self.defaults_lk = PyDict_New()
        self.slots = PyList_New(0)
        self.namespaces = namespaces
        self.fields = None


    cdef inline int set_offset_member(self, PyMemberDef* member):
        return PyDict_SetItemString(self.offsets_lk, member.name, PyLong_FromSsize_t(member.offset))

    cdef inline int set_default(self, object key, object value):
        return PyDict_SetItem(self.defaults_lk, key, value)

    # based off msgspec _core.c https://github.com/jcrist/msgspec/blob/dd965dce22e5278d4935bea923441ecde31b5325/msgspec/_core.c#L5501
    @cython.nonecheck(False)
    cdef int __collect_base__(self, object base) except -1:
        cdef:
            dict tdict
            const char** invalid_methods = ["__init__", "__new__"]
            Py_ssize_t i
            _HeapStruct hbase
            Py_ssize_t defaults_offset
            object dval, offset


        if isinstance(base, HeapStructMixin):
            return 0
        
        # TODO Checking for __weakref__ and __dict__

        if not PyType_Check(base):
            PyErr_SetString(TypeError, "All base classes must be types")
            return -1

        if not isinstance(base, _HeapStruct):
            tdict = HS_GET_TYPE_DICT(base)
            
            for i in range(2):
                if PyDict_GetItemString(tdict, invalid_methods[i]) != NULL:
                    PyErr_Format(TypeError, "Base Classes Cant Define \"%s\"",  invalid_methods[i])
                    return -1

            return 0

        hbase = <_HeapStruct>base
        defaults_offset = hbase.nfields() - hbase.ndefaults()
        
        for i , field in enumerate(hbase.s_fields):
            
            dval = NODEFAULT
            if i >= defaults_offset:
                dval = <object>PyTuple_GET_ITEM(hbase.s_defaults, i - defaults_offset)

            if self.set_default(field, dval) < 0:
                return -1
            
            # TODO: kw_only fields...

            offset = PyLong_FromSsize_t(hbase.s_offsets[i])
            
            if PyDict_SetItem(self.offsets_lk, field, offset) < 0:
                return -1
            
        return 0
    
    
    cdef inline bint is_classvar(self, object ann):
        # Determines if an attribute belongs to a class variable
        # This will later help with tagging or ignoring certain annotations
        cdef:
            Py_ssize_t ann_len
            const char* ann_buf
            # object __origin__

        
        # You may not think PyUnicode_check and the 
        # rest of this is anything but useful so let me explain
        # Sometimes annotations can be stored as strings 
        #
        # class MyDirtyExample():
        #      var_1: "int"
        #      var_2: "ClassVar[str]" = "I am a class variable"
        # We want to make sure ClassVar is picked up so perform the check...

        if PyUnicode_Check(ann):
            ann_buf = unicode_str_and_size(ann, &ann_len)
            if ann_len < 8: 
                return False
            elif (memcmp(ann_buf, b"ClassVar", 8)) == 0:
                # 91 is '[' if I used '[' Cython says 
                # "Unicode literals do not support coercion 
                # to C types other than Py_UNICODE/Py_UCS4 
                # (for characters) or Py_UNICODE* (for strings)." 
                if ann_len != 8 and ann_buf[8] != 91: 
                    return False
                return isinstance(ann, ClassVarType)
            elif ann_len < 15:
                return False
            elif (memcmp(ann_buf, b"typing.ClassVar", 15)) == 0:
                if ann_len != 15 and ann_buf[15] != 91:
                    return False
                return isinstance(ann, ClassVarType)
        
        elif isinstance(ann, ClassVarType):
            return True
        
        elif isinstance(ann, GenericAliasType):
            if not hasattr(ann, "__origin__"):
                return False
            return isinstance(getattr(ann, "__origin__"), ClassVarType)

        return False


    cdef int __process_default__(self, object name, _HeapStruct base) except -1:
        cdef PyObject* obj = PyDict_GetItem(self.namespaces, name)
        cdef object _obj

        if obj == NULL:
            return self.set_default(name, NODEFAULT)
        
        _obj = <object>getattr(base, name)

        if isinstance(_obj, Field):
            return self.set_default(name, (<Field>_obj).__get_default__())
        
        # The Object is the default, Unless it's a builtin
        elif _obj is dict:
            return self.set_default(name, Factory(_obj))
        elif _obj is list:
            return self.set_default(name, Factory(_obj))
        elif _obj is set:
            return self.set_default(name, Factory(_obj))
        elif _obj is frozenset:
            return self.set_default(name, Factory(_obj))
        elif _obj is bytearray:
            return self.set_default(name, Factory(_obj))
        else:
            # Object is already in a default-state...
            return self.set_default(name, _obj)


    
    cdef int __process_defaults__(self, _HeapStruct base) except -1:
        cdef:
            dict annotations
            object field, value, dval
            

        annotations = base.__annotations__
        if not isinstance(annotations, dict):
            PyErr_SetString(TypeError, "__annotations__ must be a \"dict\"")
            return -1
 
        for field, value in annotations.items():
            # TODO: Invalid Names...
            # NOTE: Will let that slide until 
            # SQLTable extension has been developed

            if self.is_classvar(value): continue

            if (PyDict_GetItem(self.defaults_lk, field) == NULL):
                if PyList_Append(self.slots, field) < 0:
                    return -1
            
            if self.__process_default__(field, base) < 0:
                return -1

        return 0


    cdef int __create_fields__(self, _HeapStruct base) except -1:
        cdef:
            Py_ssize_t nfields = len(self.defaults_lk)
            Py_ssize_t field_index = 0
            object field, default_val

        self.fields = PyTuple_New(nfields)
        self.defaults = PyList_New(0)

        for field, default_val in self.defaults_lk.items():

            PyTuple_SET_ITEM(self.fields, field_index, field)
            if default_val == NODEFAULT:
                if len(self.defaults):
                    raise RuntimeError("Couldn't Follow fields upon Field Creation Ending at '%U'", field)
            else:
                if PyList_Append(self.defaults, default_val) < 0:
                    return -1
            
            field_index += 1 

        self.defaults = PyList_AsTuple(self.defaults)
        self.match_args = self.defaults[0: nfields]
        
        # TODO: __weakref__, __dict__
        
        if PyList_Sort(self.slots) < 0: 
            return -1
        
        return PyDict_SetItemString(self.namespaces, "__slots__", PyList_AsTuple(self.slots))




    cdef Py_ssize_t* __construct_offsets__(self, _HeapStruct base) except NULL:
        cdef: 
            PyMemberDef* members = HeapStruct_GET_MEMBERS(base)
            Py_ssize_t* offsets
            Py_ssize_t i
            object field
            PyObject* _item

        for _ in range(Py_SIZE(base)):
            if self.set_offset_member(members) < 0:
                return NULL
            members += 1
        
        offsets = <Py_ssize_t*>PyMem_Malloc(base.nfields() * sizeof(Py_ssize_t))
        if offsets is NULL:
            PyErr_NoMemory()
            return NULL

        for i, field in enumerate(base.s_fields):

            _item = PyDict_GetItem(self.offsets_lk, field)
            if _item == NULL:
                raise RuntimeError("Failed To get Offset of %U" % field)
            
            offsets[i] = PyLong_AsSsize_t(<object>_item)

        return offsets
        
    





class HeapStructMeta(_HeapStruct):

    @classmethod
    def __new__(
        cls, str __name, tuple __bases, dict __namespace, **kwds):
        cdef: 
            InternalInfo info = InternalInfo(namespaces=__namespace)
            object base
        
        if not __bases:
            return

        for base in __bases:
            info.__collect_base__(base)
        
        print("DEBUG: __process_defaults__")
        info.__process_defaults__(self)
        info.__create_fields__(self)
        
        print("DEBUG: __slots__")
        self.__slots__ = info.namespaces["__slots__"]
        self.s_offsets = info.__construct_offsets__(self)
        print("DEBUG: __done__")

        # TODO: PyObject_GenericSetAttr

    
    def __init__(self, *args, **kw):
        cdef object arg
        cdef Py_ssize_t i
        for i, arg in enumerate(args):
            self.__set_address__(i, arg)
        
        for k, v in kw.items():
            i = self.__find_field_location__(k)
            if i < 0:
                raise TypeError("kwarg %U was not properly defined", k)
            self.__set_address__(i, arg)
    

    
    

                


        






