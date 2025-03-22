#ifndef __HEAPSTRUCT_H__
#define __HEAPSTRUCT_H__

#include "Python.h"

#pragma once

/**
 * Inspired by msgspec but is just a dataclass but with no serlizers included... 
 */

/** Allow Extending in C / C++ or pybind11 */
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#define HeapStruct_GET_MEMBERS(etype) \
    ((PyMemberDef *)(((char *)(etype)) + Py_TYPE(etype)->tp_basicsize))

#define HeapStruct_GET_FIRST_SLOT(obj) \
    *((PyObject **)((char *)(obj) + sizeof(PyObject))) \

#define HeapStruct_SET_FIRST_SLOT(obj, val) \
    HeapStruct_GET_FIRST_SLOT(obj) = (val)


typedef struct _HeapStructMeta{
    PyHeapTypeObject base;
    PyObject *struct_fields;
    PyObject *struct_defaults;
    Py_ssize_t *struct_offsets;
} HeapStructMeta;

// Further Cython Declaration
typedef struct _HeapStructMeta PyHeapStructMeta;

static PyTypeObject HeapStructMetaType;
static PyTypeObject HeapStructMixinType;

#define HeapStructMeta_GET_FIELDS(s) (((HeapStructMeta *)(s))->struct_fields);
#define HeapStructMeta_GET_NFIELDS(s) (PyTuple_GET_SIZE((((HeapStructMeta *)(s))->struct_fields)));
#define HeapStructMeta_GET_DEFAULTS(s) (((HeapStructMeta *)(s))->struct_defaults);
#define HeapStructMeta_GET_OFFSETS(s) (((HeapStructMeta *)(s))->struct_offsets);



#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif // __HEAPSTRUCT_H__