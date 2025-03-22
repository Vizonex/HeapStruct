from typing_extensions import dataclass_transform

@dataclass_transform()
class HeapStructMeta(type):
    __signature__: str
    __struct_defaults__: tuple
    __struct_fields__: tuple
    
@dataclass_transform()
class HeapStruct(metaclass=HeapStructMeta):
    """A Structure that is allocated on the heap.
    Hence the name, `HeapStruct`. It's basetype in c is `PyHeapTypeObject`"""
    __signature__:str
    __struct_defaults__: tuple
    __struct_fields__: tuple
    


