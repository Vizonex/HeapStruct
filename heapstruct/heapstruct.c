// This is essentially a fork of quickle and msgspec

#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "datetime.h"
#include "structmember.h"
#include "heapstruct.h"


#include <stdio.h>
#include <stddef.h>

// #define HEAPSTRUCT_ENABLE_DEBUG

#ifdef HEAPSTRUCT_ENABLE_DEBUG
    #define HEAPSTRUCT_DEBUG(TEXT) printf("[HEAPSTRUCT DEBUG] %s\n", TEXT) 
#else 
    // Do Nothing...
    #define HEAPSTRUCT_DEBUG(TEXT) 
#endif


#define HEAPSTRUCT_VERSION "0.0.1"
#define HEAPSTRUCT_AUTHOR "Vizonex"


#define OBJ_IS_GC(x) \
    (PyType_IS_GC(Py_TYPE(x)) && \
     (!PyTuple_CheckExact(x) || IS_TRACKED(x)))

#define IS_TRACKED  PyObject_GC_IsTracked
#define CALL_ONE_ARG(fn, arg) PyObject_CallOneArg((fn), (arg))

PyDoc_STRVAR(heapstruct__doc__, "`heapstruct` A Simple CPython Dataclass Library inspired and forked from quickle & msgspec,"\
    " \nThis Module is made for reasearch purposes on how C Dataclasses work and behave" \
    "\nThe Goal is to someday merge the code into Cython or Rust (PyO3) and without any problems\n"\
    " allowing for easier maitenence and speed. Know that this project is not production ready and may have sneaky bugs..."
);

typedef struct _HeapStructState {
    PyObject* HeapStructType;
    // PyObject* HeapStructMetaType;
} HeapStructState;


static PyObject*
find_keyword(PyObject *kwnames, PyObject *const *kwstack, PyObject *key)
{
    Py_ssize_t i, nkwargs;

    nkwargs = PyTuple_GET_SIZE(kwnames);
    for (i = 0; i < nkwargs; i++) {
        PyObject *kwname = PyTuple_GET_ITEM(kwnames, i);

        /* kwname == key will normally find a match in since keyword keys
           should be interned strings; if not retry below in a new loop. */
        if (kwname == key) {
            return kwstack[i];
        }
    }

    for (i = 0; i < nkwargs; i++) {
        PyObject *kwname = PyTuple_GET_ITEM(kwnames, i);
        assert(PyUnicode_Check(kwname));
        if (_PyUnicode_EQ(kwname, key)) {
            return kwstack[i];
        }
    }
    return NULL;
}



/* Forward declaration of the quickle module definition. */
static struct PyModuleDef heapstructmodule;

/* Given a module object, get its per-module state. */
static HeapStructState *
heapstruct_get_state(PyObject *module)
{
    return (HeapStructState *)PyModule_GetState(module);
}

static int
dict_discard(PyObject *dict, PyObject *key) {
    int status = PyDict_Contains(dict, key);
    if (status < 0)
        return status;
    return (status == 1) ? PyDict_DelItem(dict, key) : 0;
}


static inline void
HeapStruct_set_index(PyObject *obj, Py_ssize_t index, PyObject *val) {
    HeapStructMeta *cls;
    char *addr;
    PyObject *old;

    cls = (HeapStructMeta *)Py_TYPE(obj);
    addr = (char *)obj + cls->struct_offsets[index];
    old = *(PyObject **)addr;
    Py_XDECREF(old);
    *(PyObject **)addr = val;
}

/* Get field #index on obj. Returns a borrowed reference */
static inline PyObject*
HeapStruct_get_index(PyObject *obj, Py_ssize_t index) {
    HeapStructMeta *cls;
    char *addr;
    PyObject *val;

    cls = (HeapStructMeta *)Py_TYPE(obj);
    addr = (char *)obj + cls->struct_offsets[index];
    val = *(PyObject **)addr;
    if (val == NULL)
        PyErr_Format(PyExc_AttributeError,
                     "Struct field %R is unset",
                     PyTuple_GET_ITEM(cls->struct_fields, index));
    return val;
}


/* Find the module instance imported in the currently running sub-interpreter
   and get its state. */
static HeapStructState *
heapstruct_get_global_state()
{
    return heapstruct_get_state(PyState_FindModule(&heapstructmodule));
}

static PyObject *
HeapStructMixin_fields(PyObject *self, void *closure) {
    PyObject *out;
    out = HeapStructMeta_GET_FIELDS(Py_TYPE(self));
    Py_INCREF(out);
    return out;
}

static PyObject *
HeapStructMixin_defaults(PyObject *self, void *closure) {
    PyObject *out;
    out = HeapStructMeta_GET_DEFAULTS(Py_TYPE(self));
    Py_INCREF(out);
    return out;
}

static int HeapStructMeta_traverse(HeapStructMeta *self, visitproc visit, void *arg)
{
    Py_VISIT(self->struct_fields);
    Py_VISIT(self->struct_defaults);
    return PyType_Type.tp_traverse((PyObject *)self, visit, arg);
}

static int HeapStructMeta_clear(HeapStructMeta *self)
{
    Py_CLEAR(self->struct_fields);
    Py_CLEAR(self->struct_defaults);
    PyMem_Free(self->struct_offsets);
    return PyType_Type.tp_clear((PyObject *)self);
}

void HeapStructMeta_dealloc(HeapStructMeta *self)
{
    Py_XDECREF(self->struct_fields);
    Py_XDECREF(self->struct_defaults);
    PyType_Type.tp_dealloc((PyObject *)self);
}

static PyObject *
HeapStruct_vectorcall(PyTypeObject *cls, PyObject *const *args, size_t nargsf, PyObject *kwnames);



static PyObject* HeapHeapStructMeta_signature(HeapStructMeta *self, void *closure)
{
    Py_ssize_t nfields, ndefaults, npos, i;
    PyObject *res = NULL;
    PyObject *inspect = NULL;
    PyObject *parameter_cls = NULL;
    PyObject *parameter_empty = NULL;
    PyObject *parameter_kind = NULL;
    PyObject *signature_cls = NULL;
    PyObject *typing = NULL;
    PyObject *get_type_hints = NULL;
    PyObject *annotations = NULL;
    PyObject *parameters = NULL;
    PyObject *temp_args = NULL, *temp_kwargs = NULL;
    PyObject *field, *default_val, *parameter, *annotation;

    nfields = PyTuple_GET_SIZE(self->struct_fields);
    ndefaults = PyTuple_GET_SIZE(self->struct_defaults);
    npos = nfields - ndefaults;

    inspect = PyImport_ImportModule("inspect");
    if (inspect == NULL)
        goto cleanup;
    parameter_cls = PyObject_GetAttrString(inspect, "Parameter");
    if (parameter_cls == NULL)
        goto cleanup;
    parameter_empty = PyObject_GetAttrString(parameter_cls, "empty");
    if (parameter_empty == NULL)
        goto cleanup;
    parameter_kind = PyObject_GetAttrString(parameter_cls, "POSITIONAL_OR_KEYWORD");
    if (parameter_kind == NULL)
        goto cleanup;
    signature_cls = PyObject_GetAttrString(inspect, "Signature");
    if (signature_cls == NULL)
        goto cleanup;
    typing = PyImport_ImportModule("typing");
    if (typing == NULL)
        goto cleanup;
    get_type_hints = PyObject_GetAttrString(typing, "get_type_hints");
    if (get_type_hints == NULL)
        goto cleanup;

    annotations = PyObject_CallFunctionObjArgs(get_type_hints, self, NULL);
    if (annotations == NULL)
        goto cleanup;

    parameters = PyList_New(nfields);
    if (parameters == NULL)
        return NULL;

    temp_args = PyTuple_New(0);
    if (temp_args == NULL)
        goto cleanup;
    temp_kwargs = PyDict_New();
    if (temp_kwargs == NULL)
        goto cleanup;
    if (PyDict_SetItemString(temp_kwargs, "kind", parameter_kind) < 0)
        goto cleanup;

    for (i = 0; i < nfields; i++) {
        field = PyTuple_GET_ITEM(self->struct_fields, i);
        if (i < npos) {
            default_val = parameter_empty;
        } else {
            default_val = PyTuple_GET_ITEM(self->struct_defaults, i - npos);
        }
        annotation = PyDict_GetItem(annotations, field);
        if (annotation == NULL) {
            annotation = parameter_empty;
        }
        if (PyDict_SetItemString(temp_kwargs, "name", field) < 0)
            goto cleanup;
        if (PyDict_SetItemString(temp_kwargs, "default", default_val) < 0)
            goto cleanup;
        if (PyDict_SetItemString(temp_kwargs, "annotation", annotation) < 0)
            goto cleanup;
        parameter = PyObject_Call(parameter_cls, temp_args, temp_kwargs);
        if (parameter == NULL)
            goto cleanup;
        PyList_SET_ITEM(parameters, i, parameter);
    }
    res = PyObject_CallFunctionObjArgs(signature_cls, parameters, NULL);
cleanup:
    Py_XDECREF(inspect);
    Py_XDECREF(parameter_cls);
    Py_XDECREF(parameter_empty);
    Py_XDECREF(parameter_kind);
    Py_XDECREF(signature_cls);
    Py_XDECREF(typing);
    Py_XDECREF(get_type_hints);
    Py_XDECREF(annotations);
    Py_XDECREF(parameters);
    Py_XDECREF(temp_args);
    Py_XDECREF(temp_kwargs);
    return res;
}



static PyGetSetDef HeapStructMixin_getset[] = {
    {"__struct_fields__", (getter) HeapStructMixin_fields, NULL, "Struct fields", NULL},
    {"__struct_defaults__", (getter) HeapStructMixin_defaults, NULL, "Struct defaults", NULL},
    {NULL},
};


static int
StructMeta_traverse(HeapStructMeta *self, visitproc visit, void *arg)
{
    Py_VISIT(self->struct_fields);
    Py_VISIT(self->struct_defaults);
    return PyType_Type.tp_traverse((PyObject *)self, visit, arg);
}

static int
StructMeta_clear(HeapStructMeta *self)
{
    Py_CLEAR(self->struct_fields);
    Py_CLEAR(self->struct_defaults);
    PyMem_Free(self->struct_offsets);
    return PyType_Type.tp_clear((PyObject *)self);
}

void
StructMeta_dealloc(HeapStructMeta *self)
{
    Py_XDECREF(self->struct_fields);
    Py_XDECREF(self->struct_defaults);
    PyType_Type.tp_dealloc((PyObject *)self);
}

static PyObject*
HeapStructMeta_signature(HeapStructMeta *self, void *closure)
{
    Py_ssize_t nfields, ndefaults, npos, i;
    PyObject *res = NULL;
    PyObject *inspect = NULL;
    PyObject *parameter_cls = NULL;
    PyObject *parameter_empty = NULL;
    PyObject *parameter_kind = NULL;
    PyObject *signature_cls = NULL;
    PyObject *typing = NULL;
    PyObject *get_type_hints = NULL;
    PyObject *annotations = NULL;
    PyObject *parameters = NULL;
    PyObject *temp_args = NULL, *temp_kwargs = NULL;
    PyObject *field, *default_val, *parameter, *annotation;

    nfields = PyTuple_GET_SIZE(self->struct_fields);
    ndefaults = PyTuple_GET_SIZE(self->struct_defaults);
    npos = nfields - ndefaults;

    inspect = PyImport_ImportModule("inspect");
    if (inspect == NULL)
        goto cleanup;
    parameter_cls = PyObject_GetAttrString(inspect, "Parameter");
    if (parameter_cls == NULL)
        goto cleanup;
    parameter_empty = PyObject_GetAttrString(parameter_cls, "empty");
    if (parameter_empty == NULL)
        goto cleanup;
    parameter_kind = PyObject_GetAttrString(parameter_cls, "POSITIONAL_OR_KEYWORD");
    if (parameter_kind == NULL)
        goto cleanup;
    signature_cls = PyObject_GetAttrString(inspect, "Signature");
    if (signature_cls == NULL)
        goto cleanup;
    typing = PyImport_ImportModule("typing");
    if (typing == NULL)
        goto cleanup;
    get_type_hints = PyObject_GetAttrString(typing, "get_type_hints");
    if (get_type_hints == NULL)
        goto cleanup;

    annotations = PyObject_CallFunctionObjArgs(get_type_hints, self, NULL);
    if (annotations == NULL)
        goto cleanup;

    parameters = PyList_New(nfields);
    if (parameters == NULL)
        return NULL;

    temp_args = PyTuple_New(0);
    if (temp_args == NULL)
        goto cleanup;
    temp_kwargs = PyDict_New();
    if (temp_kwargs == NULL)
        goto cleanup;
    if (PyDict_SetItemString(temp_kwargs, "kind", parameter_kind) < 0)
        goto cleanup;

    for (i = 0; i < nfields; i++) {
        field = PyTuple_GET_ITEM(self->struct_fields, i);
        if (i < npos) {
            default_val = parameter_empty;
        } else {
            default_val = PyTuple_GET_ITEM(self->struct_defaults, i - npos);
        }
        annotation = PyDict_GetItem(annotations, field);
        if (annotation == NULL) {
            annotation = parameter_empty;
        }
        if (PyDict_SetItemString(temp_kwargs, "name", field) < 0)
            goto cleanup;
        if (PyDict_SetItemString(temp_kwargs, "default", default_val) < 0)
            goto cleanup;
        if (PyDict_SetItemString(temp_kwargs, "annotation", annotation) < 0)
            goto cleanup;
        parameter = PyObject_Call(parameter_cls, temp_args, temp_kwargs);
        if (parameter == NULL)
            goto cleanup;
        PyList_SET_ITEM(parameters, i, parameter);
    }
    res = PyObject_CallFunctionObjArgs(signature_cls, parameters, NULL);
cleanup:
    Py_XDECREF(inspect);
    Py_XDECREF(parameter_cls);
    Py_XDECREF(parameter_empty);
    Py_XDECREF(parameter_kind);
    Py_XDECREF(signature_cls);
    Py_XDECREF(typing);
    Py_XDECREF(get_type_hints);
    Py_XDECREF(annotations);
    Py_XDECREF(parameters);
    Py_XDECREF(temp_args);
    Py_XDECREF(temp_kwargs);
    return res;
}


static PyMemberDef HeapStructMeta_members[] = {
    {"__struct_fields__", T_OBJECT_EX, offsetof(HeapStructMeta, struct_fields), READONLY, "Struct fields"},
    {"__struct_defaults__", T_OBJECT_EX, offsetof(HeapStructMeta, struct_defaults), READONLY, "Struct defaults"},
    {NULL},
};

static PyGetSetDef HeapStructMeta_getset[] = {
    {"__signature__", (getter) HeapStructMeta_signature, NULL, NULL, NULL},
    {NULL},
};




static PyObject *
HeapStruct_richcompare(PyObject *self, PyObject *other, int op) {
    int status;
    PyObject *left, *right;
    Py_ssize_t nfields, i;

    if (!(Py_TYPE(Py_TYPE(other)) == &HeapStructMetaType)) {
        Py_RETURN_NOTIMPLEMENTED;
    }
    if (op != Py_EQ && op != Py_NE) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    status = Py_TYPE(self) == Py_TYPE(other);
    if (status == 0)
        goto done;

    nfields = HeapStructMeta_GET_NFIELDS(Py_TYPE(self));

    for (i = 0; i < nfields; i++) {
        left = HeapStruct_get_index(self, i);
        if (left == NULL)
            return NULL;
        right = HeapStruct_get_index(other, i);
        if (right == NULL)
            return NULL;
        Py_INCREF(left);
        Py_INCREF(right);
        status = PyObject_RichCompareBool(left, right, Py_EQ);
        Py_DECREF(left);
        Py_DECREF(right);
        if (status < 0)
            return NULL;
        if (status == 0)
            goto done;
    }
done:
    if (status == ((op == Py_EQ) ? 1 : 0)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}


static PyObject *
HeapStruct_copy(PyObject *self, PyObject *args)
{
    Py_ssize_t i, nfields;
    PyObject *val, *res = NULL;

    res = Py_TYPE(self)->tp_alloc(Py_TYPE(self), 0);
    if (res == NULL)
        return NULL;

    nfields = HeapStructMeta_GET_NFIELDS(Py_TYPE(self));
    for (i = 0; i < nfields; i++) {
        val = HeapStruct_get_index(self, i);
        if (val == NULL)
            goto error;
        Py_INCREF(val);
        HeapStruct_set_index(res, i, val);
    }
    /* If self is untracked, then copy is untracked */
    if (PyObject_IS_GC(self) && !IS_TRACKED(self))
        PyObject_GC_UnTrack(res);
    return res;
error:
    Py_DECREF(res);
    return NULL;
}

static PyObject *
HeapStruct_reduce(PyObject *self, PyObject *args)
{
    Py_ssize_t i, nfields;
    PyObject *values, *val;

    nfields = HeapStructMeta_GET_NFIELDS(Py_TYPE(self));
    values = PyTuple_New(nfields);
    if (values == NULL)
        return NULL;
    for (i = 0; i < nfields; i++) {
        val = HeapStruct_get_index(self, i);
        if (val == NULL)
            goto error;
        Py_INCREF(val);
        PyTuple_SET_ITEM(values, i, val);
    }
    return PyTuple_Pack(2, Py_TYPE(self), values);
error:
    Py_XDECREF(values);
    return NULL;
}


static int
HeapStruct_setattro(PyObject *self, PyObject *key, PyObject *value) {
    if (PyObject_GenericSetAttr(self, key, value) < 0)
        return -1;
    if (value != NULL && OBJ_IS_GC(value) && !IS_TRACKED(self))
        PyObject_GC_Track(self);
    return 0;
}


static PyObject *
HeapStruct_repr(PyObject *self) {
    int recursive;
    Py_ssize_t nfields, i;
    PyObject *parts = NULL, *empty = NULL, *out = NULL;
    PyObject *part, *fields, *field, *val;

    recursive = Py_ReprEnter(self);
    if (recursive != 0) {
        out = (recursive < 0) ? NULL : PyUnicode_FromString("...");
        goto cleanup;
    }

    fields = HeapStructMeta_GET_FIELDS(Py_TYPE(self));
    nfields = PyTuple_GET_SIZE(fields);
    if (nfields == 0) {
        out = PyUnicode_FromFormat("%s()", Py_TYPE(self)->tp_name);
        goto cleanup;
    }

    parts = PyList_New(nfields + 1);
    if (parts == NULL)
        goto cleanup;

    part = PyUnicode_FromFormat("%s(", Py_TYPE(self)->tp_name);
    if (part == NULL)
        goto cleanup;
    PyList_SET_ITEM(parts, 0, part);

    for (i = 0; i < nfields; i++) {
        field = PyTuple_GET_ITEM(fields, i);
        val = HeapStruct_get_index(self, i);
        if (val == NULL)
            goto cleanup;

        if (i == (nfields - 1)) {
            part = PyUnicode_FromFormat("%U=%R)", field, val);
        } else {
            part = PyUnicode_FromFormat("%U=%R, ", field, val);
        }
        if (part == NULL)
            goto cleanup;
        PyList_SET_ITEM(parts, i + 1, part);
    }
    empty = PyUnicode_FromString("");
    if (empty == NULL)
        goto cleanup;
    out = PyUnicode_Join(empty, parts);

cleanup:
    Py_XDECREF(parts);
    Py_XDECREF(empty);
    Py_ReprLeave(self);
    return out;
}



static PyMethodDef HeapStruct_methods[] = {
    {"__copy__", HeapStruct_copy, METH_NOARGS, "copy a struct"},
    {"__reduce__", HeapStruct_reduce, METH_NOARGS, "reduce a struct"},
    {NULL, NULL},
};

static PyTypeObject HeapStructMixinType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_heapstruct._HeapStructMixin",
    .tp_basicsize = 0,
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_setattro = HeapStruct_setattro,
    .tp_repr = HeapStruct_repr,
    .tp_richcompare = HeapStruct_richcompare,
    .tp_methods = HeapStruct_methods,
    .tp_getset = HeapStructMixin_getset,
};


static PyObject *
maybe_deepcopy_default(PyObject *obj, int *is_copy) {
    HeapStructState *st;
    PyObject *copy = NULL, *deepcopy = NULL, *res = NULL;
    PyTypeObject *type = Py_TYPE(obj);

    /* Known non-collection types */
    if (obj == Py_None || obj == Py_False || obj == Py_True ||
        type == &PyLong_Type || type == &PyFloat_Type ||
        type == &PyComplex_Type || type == &PyBytes_Type ||
        type == &PyUnicode_Type || type == &PyByteArray_Type ||
        type == &PyPickleBuffer_Type
    ) {
        return obj;
    }
    else if (type == &PyTuple_Type && (PyTuple_GET_SIZE(obj) == 0)) {
        return obj;
    }
    else if (type == &PyFrozenSet_Type && PySet_GET_SIZE(obj) == 0) {
        return obj;
    }
    else if (type == PyDateTimeAPI->DeltaType ||
             type == PyDateTimeAPI->DateTimeType ||
             type == PyDateTimeAPI->DateType ||
             type == PyDateTimeAPI->TimeType
    ) {
        return obj;
    }
    st = heapstruct_get_global_state();
    // if (type == st->TimeZoneType || type == st->ZoneInfoType) {
    //     return obj;
    // }
    // else if (PyType_IsSubtype(type, st->EnumType)) {
    //     return obj;
    // }

    if (is_copy != NULL)
        *is_copy = 1;

    /* Fast paths for known empty collections */
    if (type == &PyDict_Type && PyDict_Size(obj) == 0) {
        return PyDict_New();
    }
    else if (type == &PyList_Type && PyList_GET_SIZE(obj) == 0) {
        return PyList_New(0);
    }
    else if (type == &PySet_Type && PySet_GET_SIZE(obj) == 0) {
        return PySet_New(NULL);
    }
    /* More complicated, invoke full deepcopy */
    copy = PyImport_ImportModule("copy");
    if (copy == NULL)
        goto cleanup;
    deepcopy = PyObject_GetAttrString(copy, "deepcopy");
    if (deepcopy == NULL)
        goto cleanup;
    res = PyObject_CallFunctionObjArgs(deepcopy, obj, NULL);
cleanup:
    Py_XDECREF(copy);
    Py_XDECREF(deepcopy);
    return res;
}


static PyObject *
HeapStruct_vectorcall(PyTypeObject *cls, PyObject *const *args, size_t nargsf, PyObject *kwnames) {
    PyObject *self, *fields, *defaults, *field, *val;
    Py_ssize_t nargs, nkwargs, nfields, ndefaults, npos, i;
    int is_copy, should_untrack;

    self = cls->tp_alloc(cls, 0);
    if (self == NULL)
        return NULL;

    fields = HeapStructMeta_GET_FIELDS(Py_TYPE(self));
    defaults = HeapStructMeta_GET_DEFAULTS(Py_TYPE(self));

    nargs = PyVectorcall_NARGS(nargsf);
    nkwargs = (kwnames == NULL) ? 0 : PyTuple_GET_SIZE(kwnames);
    ndefaults = PyTuple_GET_SIZE(defaults);
    nfields = PyTuple_GET_SIZE(fields);
    npos = nfields - ndefaults;

    if (nargs > nfields) {
        PyErr_SetString(
            PyExc_TypeError,
            "Extra positional arguments provided"
        );
        return NULL;
    }

    should_untrack = PyObject_IS_GC(self);

    for (i = 0; i < nfields; i++) {
        is_copy = 0;
        field = PyTuple_GET_ITEM(fields, i);
        val = (nkwargs == 0) ? NULL : find_keyword(kwnames, args + nargs, field);
        if (val != NULL) {
            if (i < nargs) {
                PyErr_Format(
                    PyExc_TypeError,
                    "Argument '%U' given by name and position",
                    field
                );
                return NULL;
            }
            nkwargs -= 1;
        }
        else if (i < nargs) {
            val = args[i];
        }
        else if (i < npos) {
            PyErr_Format(
                PyExc_TypeError,
                "Missing required argument '%U'",
                field
            );
            return NULL;
        }
        else {
            val = maybe_deepcopy_default(PyTuple_GET_ITEM(defaults, i - npos), &is_copy);
            if (val == NULL)
                return NULL;
        }
        HeapStruct_set_index(self, i, val);
        if (!is_copy)
            Py_INCREF(val);
        if (should_untrack) {
            should_untrack = !OBJ_IS_GC(val);
        }
    }
    if (nkwargs > 0) {
        PyErr_SetString(
            PyExc_TypeError,
            "Extra keyword arguments provided"
        );
        return NULL;
    }
    if (should_untrack)
        PyObject_GC_UnTrack(self);
    return self;
}


PyObject* HeapStructMeta_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    HeapStructMeta *cls = NULL;
    PyObject *name = NULL, *bases = NULL, *namespaces = NULL;
    PyObject *arg_fields = NULL, *kwarg_fields = NULL, *new_dict = NULL, *new_args = NULL;
    PyObject *fields = NULL, *defaults = NULL, *offsets_lk = NULL, *offset = NULL, *slots = NULL, *slots_list = NULL;
    PyObject *base, *base_fields, *base_defaults, *annotations;
    PyObject *default_val, *field;
    Py_ssize_t nfields, ndefaults, i, j, k;
    Py_ssize_t *offsets = NULL, *base_offsets;

    /* Parse arguments: (name, bases, dict) */
    if (!PyArg_ParseTuple(args, "UO!O!:StructMeta.__new__", &name, &PyTuple_Type,
                          &bases, &PyDict_Type, &namespaces))
        return NULL;

    /* Parse arguments: (name, bases, dict) */
    if (PyDict_GetItemString(namespaces, "__init__") != NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Struct types cannot define __init__");
        return NULL;
    }
    if (PyDict_GetItemString(namespaces, "__new__") != NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Struct types cannot define __new__");
        return NULL;
    }
    if (PyDict_GetItemString(namespaces, "__slots__") != NULL)
    {
        PyErr_SetString(PyExc_TypeError, "Struct types cannot define __slots__");
        return NULL;
    }

    arg_fields = PyDict_New();
    if (arg_fields == NULL)
        goto error;
    kwarg_fields = PyDict_New();
    if (kwarg_fields == NULL)
        goto error;
    offsets_lk = PyDict_New();
    if (offsets_lk == NULL)
        goto error;

    for (i = PyTuple_GET_SIZE(bases) - 1; i >= 0; i--)
    {
        base = PyTuple_GET_ITEM(bases, i);
        if ((PyTypeObject *)base == &HeapStructMixinType)
        {
            continue;
        }

        if (!(PyType_Check(base) && (Py_TYPE(base) == &HeapStructMetaType)))
        {
            PyErr_SetString(
                PyExc_TypeError,
                "All base classes must be subclasses of heapstruct.HeapStruct");
            goto error;
        }
        base_fields = HeapStructMeta_GET_FIELDS(base);
        base_defaults = HeapStructMeta_GET_DEFAULTS(base);
        base_offsets = HeapStructMeta_GET_OFFSETS(base);
        nfields = PyTuple_GET_SIZE(base_fields);
        ndefaults = PyTuple_GET_SIZE(base_defaults);
        for (j = 0; j < nfields; j++)
        {
            field = PyTuple_GET_ITEM(base_fields, j);
            if (j < (nfields - ndefaults))
            {
                if (PyDict_SetItem(arg_fields, field, Py_None) < 0)
                    goto error;
                if (dict_discard(kwarg_fields, field) < 0)
                    goto error;
            }
            else
            {
                default_val = PyTuple_GET_ITEM(base_defaults, (j + ndefaults - nfields));
                if (PyDict_SetItem(kwarg_fields, field, default_val) < 0)
                    goto error;
                if (dict_discard(arg_fields, field) < 0)
                    goto error;
            }
            offset = PyLong_FromSsize_t(base_offsets[j]);
            if (offset == NULL)
                goto error;
            if (PyDict_SetItem(offsets_lk, field, offset) < 0)
                goto error;
            Py_DECREF(offset);
        }
    }

    new_dict = PyDict_Copy(namespaces);
    if (new_dict == NULL)
        goto error;
    slots_list = PyList_New(0);
    if (slots_list == NULL)
        goto error;

    annotations = PyDict_GetItemString(namespaces, "__annotations__");
    if (annotations != NULL)
    {
        if (!PyDict_Check(annotations))
        {
            PyErr_SetString(PyExc_TypeError, "__annotations__ must be a dict");
            goto error;
        }

        i = 0;
        while (PyDict_Next(annotations, &i, &field, NULL))
        {
            if (!PyUnicode_CheckExact(field))
            {
                PyErr_SetString(
                    PyExc_TypeError,
                    "__annotations__ keys must be strings");
                goto error;
            }

            /* If the field is new, add it to slots */
            if (PyDict_GetItem(arg_fields, field) == NULL && PyDict_GetItem(kwarg_fields, field) == NULL)
            {
                if (PyList_Append(slots_list, field) < 0)
                    goto error;
            }

            default_val = PyDict_GetItem(new_dict, field);
            if (default_val != NULL)
            {
                if (dict_discard(arg_fields, field) < 0)
                    goto error;
                if (PyDict_SetItem(kwarg_fields, field, default_val) < 0)
                    goto error;
                if (dict_discard(new_dict, field) < 0)
                    goto error;
            }
            else
            {
                if (dict_discard(kwarg_fields, field) < 0)
                    goto error;
                if (PyDict_SetItem(arg_fields, field, Py_None) < 0)
                    goto error;
            }
        }
    }

    fields = PyTuple_New(PyDict_Size(arg_fields) + PyDict_Size(kwarg_fields));
    if (fields == NULL)
        goto error;
    defaults = PyTuple_New(PyDict_Size(kwarg_fields));

    i = 0;
    j = 0;
    while (PyDict_Next(arg_fields, &i, &field, NULL))
    {
        Py_INCREF(field);
        PyTuple_SET_ITEM(fields, j, field);
        j++;
    }
    i = 0;
    k = 0;
    while (PyDict_Next(kwarg_fields, &i, &field, &default_val))
    {
        Py_INCREF(field);
        PyTuple_SET_ITEM(fields, j, field);
        Py_INCREF(default_val);
        PyTuple_SET_ITEM(defaults, k, default_val);
        j++;
        k++;
    }
    Py_CLEAR(arg_fields);
    Py_CLEAR(kwarg_fields);

    if (PyList_Sort(slots_list) < 0)
        goto error;

    slots = PyList_AsTuple(slots_list);
    if (slots == NULL)
        goto error;
    Py_CLEAR(slots_list);

    if (PyDict_SetItemString(new_dict, "__slots__", slots) < 0)
        goto error;
    Py_CLEAR(slots);

    new_args = Py_BuildValue("(OOO)", name, bases, new_dict);
    if (new_args == NULL)
        goto error;

    cls = (HeapStructMeta *)PyType_Type.tp_new(type, new_args, kwargs);
    if (cls == NULL)
        goto error;
    ((PyTypeObject *)cls)->tp_vectorcall = (vectorcallfunc)HeapStruct_vectorcall;
    Py_CLEAR(new_args);

    PyMemberDef *mp = HeapStruct_GET_MEMBERS(cls);
    for (i = 0; i < Py_SIZE(cls); i++, mp++)
    {
        offset = PyLong_FromSsize_t(mp->offset);
        if (offset == NULL)
            goto error;
        if (PyDict_SetItemString(offsets_lk, mp->name, offset) < 0)
            goto error;
    }
    offsets = PyMem_New(Py_ssize_t, PyTuple_GET_SIZE(fields));
    if (offsets == NULL)
        goto error;
    for (i = 0; i < PyTuple_GET_SIZE(fields); i++)
    {
        field = PyTuple_GET_ITEM(fields, i);
        offset = PyDict_GetItem(offsets_lk, field);
        if (offset == NULL)
        {
            PyErr_Format(PyExc_RuntimeError, "Failed to get offset for %R", field);
            goto error;
        }
        offsets[i] = PyLong_AsSsize_t(offset);
    }
    Py_CLEAR(offsets_lk);

    cls->struct_fields = fields;
    cls->struct_defaults = defaults;
    cls->struct_offsets = offsets;
    return (PyObject *)cls;
error:
    Py_XDECREF(arg_fields);
    Py_XDECREF(kwarg_fields);
    Py_XDECREF(fields);
    Py_XDECREF(defaults);
    Py_XDECREF(new_dict);
    Py_XDECREF(slots_list);
    Py_XDECREF(new_args);
    Py_XDECREF(offsets_lk);
    Py_XDECREF(offset);
    if (offsets != NULL)
        PyMem_Free(offsets);
    return NULL;
}

static PyTypeObject HeapStructMetaType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_heapstruct.HeapStructMeta",
    .tp_basicsize = sizeof(HeapStructMeta),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_TYPE_SUBCLASS | Py_TPFLAGS_HAVE_GC | _Py_TPFLAGS_HAVE_VECTORCALL,
    .tp_new = HeapStructMeta_new,
    .tp_dealloc = (destructor) StructMeta_dealloc,
    .tp_clear = (inquiry) StructMeta_clear,
    .tp_traverse = (traverseproc) StructMeta_traverse,
    .tp_members = HeapStructMeta_members,
    .tp_getset = HeapStructMeta_getset,
    .tp_call = PyVectorcall_Call,
    .tp_vectorcall_offset = offsetof(PyTypeObject, tp_vectorcall),
};



static int
heapstruct_clear(PyObject *m)
{
    HeapStructState *st = heapstruct_get_state(m);
    Py_CLEAR(st->HeapStructType);
    // Py_CLEAR(st->HeapStructMetaType);
    return 0;
}

static void
heapstruct_free(PyObject *m)
{
    heapstruct_clear(m);
}

static int
heapstruct_traverse(PyObject *m, visitproc visit, void *arg)
{
    HeapStructState *st = heapstruct_get_state(m);
    Py_VISIT(st->HeapStructType);
    // Py_VISIT(st->HeapStructMetaType);
    return 0;
}

static struct PyModuleDef heapstructmodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_heapstruct",
    .m_doc = heapstruct__doc__,
    .m_size = sizeof(HeapStructState),
    .m_methods = NULL,
    .m_traverse = heapstruct_traverse,
    .m_clear = heapstruct_clear,
    .m_free = heapstruct_free
};

PyDoc_STRVAR(Struct__doc__, "A Structure that is allocated on the heap."\
    " Hence the name, `HeapStruct`. It's basetype in c is `PyHeapTypeObject`");


PyMODINIT_FUNC PyInit__heapstruct(){
    PyObject* m;
    HeapStructState* st;
    
    HEAPSTRUCT_DEBUG("Preparing import... LINE: 1045");
    m = PyState_FindModule(&heapstructmodule);
    if (m){
        Py_INCREF(m);
        return m;
    }
    m = PyModule_Create(&heapstructmodule);

    HEAPSTRUCT_DEBUG("Adding Constants info...");

    if (PyModule_AddStringConstant(m, "__version__", HEAPSTRUCT_VERSION) < 0){
        HEAPSTRUCT_DEBUG("EXITED AT SETTING __version__");
        return NULL;
    }
    if (PyModule_AddStringConstant(m, "__author__", HEAPSTRUCT_AUTHOR) < 0){
        HEAPSTRUCT_DEBUG("EXITED AT SETTING __author__");
        return NULL;
    }

    // BSD-3 Forbids it...
    // if (PyModule_AddStringConstant(m, "__former_author__", HEAPSTRUCT_PREVIOUS_AUTHOR) < 0){
    //     return NULL;
    // }
    HEAPSTRUCT_DEBUG("Setting HeapStructMetaType.tp_base to PyType");

    HeapStructMetaType.tp_base = &PyType_Type;
    HEAPSTRUCT_DEBUG("Checking If HeapStruct Meta is Ready....");

    if (PyType_Ready(&HeapStructMetaType) < 0){
        return NULL;
    }
    HEAPSTRUCT_DEBUG("Checking If HeapStructMixin is Ready....");
    
    if (PyType_Ready(&HeapStructMixinType) < 0){
        return NULL;
    }
    HEAPSTRUCT_DEBUG("Creating Module");
    m = PyModule_Create(&heapstructmodule);
    if (m == NULL){
        return NULL;
    }

    HEAPSTRUCT_DEBUG("Setting HeapStructMetaType To Module");
    st = heapstruct_get_state(m);
    /* Unlike Quickle or msgspec We want to actually use the HeapStructMeta Type 
     * for hacking purposes such as brute-forcing the object to work alongside 
     * sqlalchemy one of these days that is our Endgame.*/

    if (PyModule_AddObject(m, "HeapStructMeta", (PyObject*)(&HeapStructMetaType)) < 0){
        return NULL;
    }

    HEAPSTRUCT_DEBUG("Setting up HeapStruct Main Object");
    
    
    /* Initialize the Struct Type */
    st->HeapStructType = PyObject_CallFunction(
        (PyObject *)&HeapStructMetaType, "s(O){ssss}", "HeapStruct", &HeapStructMixinType,
        "__module__", "heapstruct", "__doc__", Struct__doc__
    );
    if (st->HeapStructType == NULL){
        return NULL;
    }
    HEAPSTRUCT_DEBUG("Adding HeapStruct Main Object To Module");

    Py_INCREF(st->HeapStructType);
    if (PyModule_AddObject(m, "HeapStruct", st->HeapStructType) < 0)
        return NULL;
    HEAPSTRUCT_DEBUG("DONE!");
    return m;
}
