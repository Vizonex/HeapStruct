# HeapStruct
A Msgspec inspired dataclass library Made for CPython, Python and Cython.

## What makes heapstruct Different from msgspec
- The code from msgspec is forked and is being carefully optimized. 
- Fields are subclassable making it very easy to customize and manipulate
- `__post_new__` function upon dataclass creation for tools like sqlalchemy
- `__subclass_init__` or a metaclass can be used when making your own custom base
- Only core functionality is provided meaning that json or other serlizers will have
to be written on your own. With everything being written in Cython that should make
it very easy to configure and with a Cython port being included you'll be more than
ready for such a task.
- Code can be compiled on it's own or with Cython (I hope for something simillar to what numpy does)

## When Should HeapStruct Be used
- When msgspec itself doesn't do exactly what you want
- When you want to merge an sql library like sqlalchemy or Peewee3 to it directly (Might look into how Peewee3 makes dataclasses since it's a dataclass-like sql-library written in Cython)
  this can be easily frustrating when you don't have any control over 2 libraries
- When you need customization over the `__new__` function under `__post_new__`
- Making a quick/dirty dataclass-model without the code-bloat

## TODOS
- [ ] msgspec & attrs support (Maybe another extension like the one attrs added to msgspec)
- [ ] Propose for Cython to allow cdef metaclasses under a new directive/wrapper which
  will allow heapstruct to become it's own cython library thus preventing it from being
  limited to python items such as giving `__init_subclass__` and `__new__` support
  I will be drawing out concept templates for how this can be approched in C.
