# HeapStruct 

A Forked Version of Quickle But it's just the Structure's boilerplate code to making a dataclass type object.
This Class Object is currently being utilized to research into ways that we could theoretically inject a 
BaseModel dataclass into a Cython cdef extension or Rust (PyO3) as well as Making a fully compatable subclass that can work 
with SQLAlchemy Directly. 

## Why I forked the library
- Maintenance started to slow down for the msgspec branch.
- To Reasearch, Hack and Stress Test Everything. 
The more I use it, the more mature it becomes.
- making a Fully Cython Compatable Version of Quickle & Msgspec allowing for extra customization or writing your own encoder libraries.
- To make the metaclass portion modifiable via subclassing it.
- Researching ways to make valid C Extension Metaclasses for python (This has been a long-time issue with Cython itself...)
- When it can't be made yourself, fork it.

## Currently Planned
- [x] Cython Module (pxd) `__init__.cython-3.0.pxd` & `c_heapstruct.pxd` backends
- [ ] Testing Cython extension classes to see if cdef-classes will behave appropreately if compiled

- [ ] `DictHeapMeta` a subclass of `HeapStructMeta` that allows for `__dict__` attributes (This is the key feature required for SQLAlchemy to work). This will need to be written in C alongside the already avalible `HeapstructMeta` Object 

- [ ] Cwpack Cython Extension for Decoding and Encoding HeapStructs since it's considered to be the fastest msgpack library.

- [ ] Making `Field` Objects in the CPython Code.

- [ ] Benchmarking against `Pydantic` and `SQLModel`

- [ ] adding validators but those features will be coming at the very very end, it will likely be the last feature I get around to adding...

- [ ] adding a `__post__` or `__post_new__` function which was [an idea for extending `__new__` function](https://discuss.python.org/t/add-a-post-method-equivalent-to-the-new-method-but-called-after-init/5449), this would in fact be simillar to how cython's `__cinit__` works where after the object is made, start setting class variables and is triggered right before `__init_subclasss__` is called.

- [ ] Finding ways and ideas for reataining SqlAlchemy Columns whenever  `__init__` is not used with a Decoder. (This is currently been a reoccuring issue with the [SQLTable Library](https://github.com/Vizonex/SQLTable))

- [ ] Catching up with the msgspec branch by typechecking ClassVars, Quickle doesn't have these features...

## Changes made from Quickle / Msgspec
- Made Struct's Metaclass Importable in order to try and stress-test everything and extend the limits of what was possible before.
- Renamed to Heap Named after CPython's `PyHeapObjectType`
- `__init_subclass__` is not used allowing the programmer/developer to utilize it fully. Instead we will use Mixins. Reason for doing so is to make a friendly SQLAlchemy branch.
- `__post__` or `__post_new__` feature that I'm planning to fully implement.
- If I have to go as far as what numpy does with the Cython Compiler, I will, I am exteremely relentless.
- We're more focused on providing compatability to third party libraries.
- Future Field Implementations are going to be subclassable, strict as msgspec but subclassable or we will provide ways for adding custom attributes...


## Current Reasons why Pydantic is slow compared to this branch
- They haven't figured out a way to make a metaclass in Rust PyO3 yet ,if this was solved we would have comparable competition.
Being bound to python alone slows down everything and is their current bottle-kneck. I am researching on behalf of everybody
here and I encourage people to figure out this unsolved puzzle.

