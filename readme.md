# HeapStruct 

A Forked Version of Quickle But it's just the Structure's boilerplate code to making a dataclass type object
This Class Object is currently being utilized to research ways to inject a BaseModel dataclass into 
Cython or Rust (PyO3) as well as Making a fully compatable subclass that can work with SQLAlchemy Directly. 

## Why I forked the library
- Maintenance started to slow down for the msgspec branch.
- To Reasearch, Hack and Stress Test Everything. 
The more I use it, the more mature it becomes.
- making a Cython Compatable Version of Quickle 
- To make the metaclass portion modifiable via subclassing it.


## Currently Planned
- [x] Cython Module (pxd) `__init__.cython-3.0.pxd` & `c_heapstruct.pxd` backends
- [ ] Testing Cython extension classes to see if cdef-classes will behave appropreately if compiled

- [] `DictHeapMeta` a subclass of `HeapStructMeta` that allows for `__dict__` attributes (This is the key feature required for SQLAlchemy to work). This will need to be written in C alongside the already avalible `HeapstructMeta` Object 

- [] Cwpack Cython Extension for Decoding and Encoding HeapStructs since it's considered to be the fastest msgpack library.

- [] Making `Field` Objects in the CPython Code.

- Benchmarking against `Pydantic` and `SQLModel`

- [] adding validators but those features will be coming at the very very end...

- [] adding a `__post__` or `__post_new__` function which was [an idea for extending `__new__` function](https://discuss.python.org/t/add-a-post-method-equivalent-to-the-new-method-but-called-after-init/5449), this would in fact be simillar to how cython's `__cinit__` works where after the object is made, start setting class variables and is triggered right before `__init_subclasss__` is called.

- [] Finding ways and ideas for reataining SqlAlchemy Columns whenever     `__init__` is not used with a Decoder. (This is currently been a reoccuring issue with the [SQLTable Library](https://github.com/Vizonex/SQLTable))


## Changes from Quickle / Msgspec
- Made Struct's Metaclass Importable in order to try and stress-test everything.
- Renamed to Heap Named after CPython's `PyHeapObjectType`
- `__init_subclass__` is not used allowing the programmer/developer to utilize them without the code erroring out...

- `__post__` or `__post_new__` feature.

- If I have to go as far as what numpy does with the Cython Compiler, I will. I am exteremely relentless.


