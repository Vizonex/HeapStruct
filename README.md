# HeapStruct
A Msgspec inspired dataclass library Made for CPython, Python and Cython.

## What makes heapstruct Different from msgspec
- Some of the code from msgspec is forked and carefully optimized. 
- Fields are subclassable making it very easy to customize and manipulate
- `__post_new__` function upon dataclass creation for tools like sqlalchemy
- `__subclass_init__` or a metaclass can be used when making your own custom base
- Only core functionality is provided meaning that json or other serlizers will have
to be written on your own. With everything going to be written in Cython that should make
it very easy to configure and with a Cython port being included you'll be more than
ready for such a task.
- Code can be compiled on it's own or with Cython (I hope for something simillar to what numpy does)

I will upload this code very soon as I am done with the first version of it.
