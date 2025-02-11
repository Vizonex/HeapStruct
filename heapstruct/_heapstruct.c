/** inspired by msgspec made into a Friendly CPython-API
 * it uses most msgspec's core dataclass functionalities and is attempted 
 * to be optimized */


#include <Python.h>
#include <stdbool.h>


/* This library is incomplete, if you want to help me, throw me an issue on github - Vizonex */

#ifdef __GNUC__
#define HS_LIKELY(pred) __builtin_expect(!!(pred), 1)
#define HS_UNLIKELY(pred) __builtin_expect(!!(pred), 0)
#else
#define HS_LIKELY(pred) (pred)
#define HS_UNLIKELY(pred) (pred)
#endif

#ifdef __GNUC__
#define HS_INLINE __attribute__((always_inline)) inline
#define HS_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define HS_INLINE __forceinline
#define HS_NOINLINE __declspec(noinline)
#else
#define HS_INLINE inline
#define HS_NOINLINE
#endif

#define PY310_PLUS (PY_VERSION_HEX >= 0x030a0000)
#define PY311_PLUS (PY_VERSION_HEX >= 0x030b0000)
#define PY312_PLUS (PY_VERSION_HEX >= 0x030c0000)
#define PY313_PLUS (PY_VERSION_HEX >= 0x030d0000)
#define PY314_PLUS (PY_VERSION_HEX >= 0x030e0000)


#if defined(__GNUC__)
    #define OPT_FORCE_RELOAD(x) __asm volatile("":"=m"(x)::);
#else
    #define OPT_FORCE_RELOAD(x)
#endif

#if PY312_PLUS
#define HS_GET_TYPE_DICT(a) PyType_GetDict(a)
#else
#define HS_GET_TYPE_DICT(a) ((a)->tp_dict)
#endif

#if PY313_PLUS
#define HS_UNICODE_EQ(a, b) (PyUnicode_Compare(a, b) == 0)
#else
#define HS_UNICODE_EQ(a, b) _PyUnicode_EQ(a, b)
#endif

#if PY314_PLUS
#define HS_IMMORTAL_INITIAL_REFCNT _Py_IMMORTAL_INITIAL_REFCNT
#else
#define HS_IMMORTAL_INITIAL_REFCNT _Py_IMMORTAL_REFCNT
#endif

typedef struct _HeapStruct {
    PyHeapTypeObject* base;
    Py_ssize_t* s_offsets;
    PyObject* s_fields;
    PyObject* s_defaults;

    PyObject *post_init;
    /* Defines a callable that can be called after __new__ is done called "__post_new__" */
    PyObject* post_new; 

} HeapStruct;


/* From msgspec._core.c */

typedef struct {
    uintptr_t _gc_next;
    uintptr_t _gc_prev;
} HS_PyGC_Head;

#define HS_AS_GC(o) ((HS_PyGC_Head *)(o)-1)

#define HS_IS_TRACKED(o) (HS_AS_GC(o)->_gc_next != 0)
#define HS_TYPE_IS_GC(t) (((PyTypeObject *)(t))->tp_flags & Py_TPFLAGS_HAVE_GC)
#define HS_MAYBE_TRACKED(x)  \
    (HS_TYPE_IS_GC(Py_TYPE(x)) && \
    (!PyTuple_CheckExact(x) || HS_IS_TRACKED(x)))



PyObject _NoDefaultObject;
#define NODEFAULT &_NoDefaultObject

static PyObject *
nodefault_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    if (PyTuple_GET_SIZE(args) || (kwargs && PyDict_GET_SIZE(kwargs))) {
        PyErr_SetString(PyExc_TypeError, "NoDefaultType takes no arguments");
        return NULL;
    }
    Py_INCREF(NODEFAULT);
    return NODEFAULT;
}

static PyObject *
nodefault_repr(PyObject *op)
{
    return PyUnicode_FromString("NODEFAULT");
}

static PyObject *
nodefault_reduce(PyObject *op, PyObject *args)
{
    return PyUnicode_FromString("NODEFAULT");
}




static PyTypeObject Field_Type;

typedef struct _FieldObject {
    PyObject_HEAD
    PyObject* name;
    PyObject* default;
    PyObject* default_factory;
} FieldObject;





static int is_FieldObject(PyObject* cls){
    if (Py_TYPE(cls) == &Field_Type)
        return 1;

    // Check MRO
    PyObject* mro = Py_TYPE(cls)->tp_mro;
    if (HS_LIKELY(mro)){
        Py_ssize_t i, n;
        n = PyTuple_GET_SIZE(mro);
        for (i = 0; i < n; i++){
            if (Py_TYPE(PyTuple_GET_ITEM(mro, i)) == &Field_Type){
                return 1;
            }
        }
    }
    return 0;
}



static int FieldObject_init(
    FieldObject* self,
    PyObject* default,
    PyObject* default_factory,
    PyObject* name
){
    if (name != Py_None && !PyUnicode_CheckExact(name)){
        PyErr_SetString(PyExc_TypeError, "name must be a str or None");
        return -1;
    }
    if (default != NODEFAULT && default_factory != NODEFAULT){
        PyErr_SetString(
            PyExc_TypeError, "Cannot set both `default` and `default_factory`"
        );
        return -1;
    }
    Py_XINCREF(name);
    self->name = name;
    Py_INCREF(default);
    self->default = default;
    Py_INCREF(default_factory);
    self->default_factory = default_factory;
    return 0;
};

static PyObject* FieldObject_new(
    PyTypeObject *type, PyObject *args, PyObject *kwargs
){
    FieldObject* self = (FieldObject*)type->tp_alloc(&Field_Type, 0);
    if (self == NULL) return NULL;
    self->default = Py_None;
    self->default_factory = Py_None;
    self->name = Py_None;
    return (PyObject*)self;
}

static int
FieldObject_traverse(FieldObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->default);
    Py_VISIT(self->default_factory);
    Py_VISIT(self->name);
    return 0;
}

static int FieldObject_clear(FieldObject *self){
    Py_CLEAR(self->default);
    Py_CLEAR(self->default_factory);
    Py_CLEAR(self->name);
    return 0;
}

static void FieldObject_dealloc(FieldObject *self){
    PyObject_GC_UnTrack(self);
    FieldObject_clear(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}


// This is where we go a little different from msgspec
// We retain the FieldObject for as long as possible so 
// that subclassing the Field can be used by the user before 
// initialization

static PyObject* FieldObject_get_default(FieldObject *self){
    if (self->default != NODEFAULT){
        Py_INCREF(self->default);
        return self->default;
    } else if (self->default_factory != NODEFAULT){
        PyObject* factory = self->default_factory;
        if (factory == (PyObject*)(&PyList_Type)){
            return PyList_New(0);
        }
        else if (factory == (PyObject*)(&PyDict_Type)){
            return PyDict_New();
        }
        return PyObject_CallNoArgs(factory);
    }
    return NULL;
}



static HS_INLINE int
HeapStruct_post_init(HeapStruct *hs_type, PyObject *obj) {
    if (hs_type->post_init != NULL) {
        PyObject *res = PyObject_CallOneArg(hs_type->post_init, obj);
        if (res == NULL) return -1;
        Py_DECREF(res);
    }
    return 0;
}


static PyObject* HeapStuct_get_index(
    PyObject* obj,
    Py_ssize_t index
){
    HeapStruct* cls = (HeapStruct*)Py_TYPE(obj);
    char* addr = (char*)obj + cls->s_offsets[index];
    PyObject* val = *(PyObject**)addr;
    if (val == NULL){
        PyErr_Format(PyExc_AttributeError,
        "HeapStruct Field %R is unset", PyTuple_GET_ITEM(cls->s_offsets, index));
        return NULL;
    }
    return val;
}

static void HeapStruct_set_index(
    PyObject* obj,
    PyObject* value,
    Py_ssize_t index
){
    char *addr;
    PyObject *old;
    HeapStruct* cls = (HeapStruct*)Py_TYPE(obj);
    addr = (char *)obj + cls->s_offsets[index];
    old = *(PyObject **)addr;
    Py_XDECREF(old);
    *(PyObject **)addr = value;
}

#define HS__MEMBER_AND_LENGTH(obj, member) \
    PyObject* member = obj->s_##member; \
    Py_ssize_t n##member = PyTuple_GET_SIZE(member)


HS_INLINE static PyObject * _HeapStruct_alloc(PyTypeObject *type, bool is_gc) {
    PyObject *obj;
    if (is_gc) {
        obj = PyObject_GC_New(PyObject, type);
    }
    else {
        obj = PyObject_New(PyObject, type);
    }
    if (obj == NULL) return NULL;
    /* Zero out slot fields */
    memset((char *)obj + sizeof(PyObject), '\0', type->tp_basicsize - sizeof(PyObject));
    return obj;
}

// CPython-API Version of HeapStruct_alloc

static PyObject* HeapStruct_alloc(PyTypeObject* type){
    return _HeapStruct_alloc(type, HS_TYPE_IS_GC(type));
}


static PyObject* HeapStruct_vectorcall(
    PyTypeObject *cls, PyObject *const *args, size_t nargsf, PyObject *kwnames
){
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
    Py_ssize_t nkwargs = (kwnames == NULL) ? 0: PyTuple_GET_SIZE(kwnames);

    HeapStruct* hs_type = (HeapStruct*)cls;

    HS__MEMBER_AND_LENGTH(hs_type, fields);
    HS__MEMBER_AND_LENGTH(hs_type, defaults);
    Py_ssize_t npos = nfields - ndefaults;

    bool is_gc = HS_TYPE_IS_GC(cls);
    bool untrack = is_gc;

    PyObject* self = HeapStruct_alloc(cls, is_gc);
    if (self == NULL) return NULL;


    for (Py_ssize_t i = 0; i < nargs; i++){
        PyObject* val = args[i];
        char* addr = (char*)self + hs_type->s_offsets[i];
        Py_INCREF(val);
        *(PyObject**)addr = val;
        if (untrack){
            untrack = !HS_MAYBE_TRACKED(val);
        }
    }

    for (Py_ssize_t i = 0; i < nkwargs; i++){
        // TODO: vector-call SEE Line: 7337
        char* addr;
        PyObject *val;
        Py_ssize_t field_index;
        PyObject* kwname = PyTuple_GET_ITEM(kwnames, i);

        for (field_index = 0; field_index < nfields; field_index++){
            PyObject* field = PyTuple_GET_ITEM(fields, field_index);
            if (HS_LIKELY(kwname == field)) goto kw_found;
        }

        for (field_index = 0; field_index < nfields; field_index++){
            PyObject* field = PyTuple_GET_ITEM(fields, field_index);
            if (HS_UNICODE_EQ(kwname, field)) {
                if (HS_UNLIKELY(field_index < nargs)){
                    PyErr_Format(
                        PyExc_TypeError,
                        "Argument '%U' given by name and position",
                        kwname
                    );
                    goto error;
                }
            }
            goto kw_found;
        }

        /* Unknown Keyword */
        PyErr_Format(PyExc_TypeError, "Unexpected keyword argument '%U'", kwname);
        goto error;

        kw_found:
            val = args[i + nargs];
            addr = (char *)self + hs_type->s_offsets[field_index];
            Py_INCREF(val);
            *(PyObject **)addr = val;
            if (untrack) {
                untrack = !HS_MAYBE_TRACKED(val);
            }
    }
    
    /* Fill in all missing defaults */
    if (nargs + nkwargs < nfields){
        for (Py_ssize_t field_index = nargs; field_index < nfields; field_index++) {
            char *addr = (char *)self + hs_type->s_offsets[field_index];
            if (HS_LIKELY(*(PyObject **)addr == NULL)) {
                if (HS_LIKELY(field_index >= npos)) {
                    PyObject *val = PyTuple_GET_ITEM(defaults, field_index - npos);
                    if (HS_LIKELY(val != NODEFAULT)) {
                        if (is_FieldObject(val)) {
                            val = FieldObject_get_default((FieldObject*)val);
                            if (MS_UNLIKELY(val == NULL)) goto error;
                            *(PyObject **)addr = val;
                            if (untrack) {
                                untrack = !HS_MAYBE_TRACKED(val);
                            }
                            continue;
                        }
                    } else {
                        // Value is the Default
                        if (MS_UNLIKELY(val == NULL)) goto error;
                        *(PyObject **)addr = val;
                        if (untrack){
                            untrack = !HS_MAYBE_TRACKED(val);
                        }
                    }
                }
                PyErr_Format(
                    PyExc_TypeError,
                    "Missing required argument '%U'",
                    PyTuple_GET_ITEM(fields, field_index)
                );
                goto error;
            }
        }
    }
    

    if (is_gc && !untrack){
        PyObject_GC_Track(self);
    }
    if (HeapStruct_post_init(hs_type, self) < 0) goto error;
    return self;

    error:
        Py_DECREF(self);
        return NULL;
}

// Sets a vector-call to the `__init__` function
void HeapStruct_enable_init(HeapStruct* cls){
    ((PyTypeObject*)cls)->tp_vectorcall = (vectorcallfunc)HeapStruct_vectorcall;
}


