"""
`heapstruct` A Simple CPython Dataclass Library inspired and forked from quickle & msgspec,
This Module is made for reasearch purposes on how C Dataclasses work and behave
The Goal is to someday merge the code into Cython or Rust (PyO3) and without any problems
allowing for easier maitenence and speed. Know that this project is not production ready and may have sneaky bugs...
"""
from ._heapstruct import HeapStruct, HeapStructMeta
