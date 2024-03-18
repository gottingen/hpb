// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google LLC.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "python/extension_dict.h"

#include "python/message.h"
#include "python/protobuf.h"
#include "hpb/reflection/def.h"

// -----------------------------------------------------------------------------
// ExtensionDict
// -----------------------------------------------------------------------------

typedef struct {
  PyObject_HEAD;
  PyObject* msg;  // Owning ref to our parent pessage.
} PyUpb_ExtensionDict;

PyObject* PyUpb_ExtensionDict_New(PyObject* msg) {
  PyUpb_ModuleState* state = PyUpb_ModuleState_Get();
  PyUpb_ExtensionDict* ext_dict =
      (void*)PyType_GenericAlloc(state->extension_dict_type, 0);
  ext_dict->msg = msg;
  Py_INCREF(ext_dict->msg);
  return &ext_dict->ob_base;
}

static PyObject* PyUpb_ExtensionDict_FindExtensionByName(PyObject* _self,
                                                         PyObject* key) {
  PyUpb_ExtensionDict* self = (PyUpb_ExtensionDict*)_self;
  const char* name = PyUpb_GetStrData(key);
  if (!name) {
    PyErr_Format(PyExc_TypeError, "_FindExtensionByName expect a str");
    return NULL;
  }
  const hpb_MessageDef* m = PyUpb_Message_GetMsgdef(self->msg);
  const hpb_FileDef* file = hpb_MessageDef_File(m);
  const hpb_DefPool* symtab = hpb_FileDef_Pool(file);
  const hpb_FieldDef* ext = hpb_DefPool_FindExtensionByName(symtab, name);
  if (ext) {
    return PyUpb_FieldDescriptor_Get(ext);
  } else {
    Py_RETURN_NONE;
  }
}

static PyObject* PyUpb_ExtensionDict_FindExtensionByNumber(PyObject* _self,
                                                           PyObject* arg) {
  PyUpb_ExtensionDict* self = (PyUpb_ExtensionDict*)_self;
  const hpb_MessageDef* m = PyUpb_Message_GetMsgdef(self->msg);
  const hpb_MiniTable* l = hpb_MessageDef_MiniTable(m);
  const hpb_FileDef* file = hpb_MessageDef_File(m);
  const hpb_DefPool* symtab = hpb_FileDef_Pool(file);
  const hpb_ExtensionRegistry* reg = hpb_DefPool_ExtensionRegistry(symtab);
  int64_t number = PyLong_AsLong(arg);
  if (number == -1 && PyErr_Occurred()) return NULL;
  const hpb_MiniTableExtension* ext =
      (hpb_MiniTableExtension*)hpb_ExtensionRegistry_Lookup(reg, l, number);
  if (ext) {
    const hpb_FieldDef* f = hpb_DefPool_FindExtensionByMiniTable(symtab, ext);
    return PyUpb_FieldDescriptor_Get(f);
  } else {
    Py_RETURN_NONE;
  }
}

static void PyUpb_ExtensionDict_Dealloc(PyUpb_ExtensionDict* self) {
  PyUpb_Message_ClearExtensionDict(self->msg);
  Py_DECREF(self->msg);
  PyUpb_Dealloc(self);
}

static PyObject* PyUpb_ExtensionDict_RichCompare(PyObject* _self,
                                                 PyObject* _other, int opid) {
  // Only equality comparisons are implemented.
  if (opid != Py_EQ && opid != Py_NE) {
    Py_INCREF(Py_NotImplemented);
    return Py_NotImplemented;
  }
  PyUpb_ExtensionDict* self = (PyUpb_ExtensionDict*)_self;
  bool equals = false;
  if (PyObject_TypeCheck(_other, Py_TYPE(_self))) {
    PyUpb_ExtensionDict* other = (PyUpb_ExtensionDict*)_other;
    equals = self->msg == other->msg;
  }
  bool ret = opid == Py_EQ ? equals : !equals;
  return PyBool_FromLong(ret);
}

static int PyUpb_ExtensionDict_Contains(PyObject* _self, PyObject* key) {
  PyUpb_ExtensionDict* self = (PyUpb_ExtensionDict*)_self;
  const hpb_FieldDef* f = PyUpb_Message_GetExtensionDef(self->msg, key);
  if (!f) return -1;
  hpb_Message* msg = PyUpb_Message_GetIfReified(self->msg);
  if (!msg) return 0;
  if (hpb_FieldDef_IsRepeated(f)) {
    hpb_MessageValue val = hpb_Message_GetFieldByDef(msg, f);
    return hpb_Array_Size(val.array_val) > 0;
  } else {
    return hpb_Message_HasFieldByDef(msg, f);
  }
}

static Py_ssize_t PyUpb_ExtensionDict_Length(PyObject* _self) {
  PyUpb_ExtensionDict* self = (PyUpb_ExtensionDict*)_self;
  hpb_Message* msg = PyUpb_Message_GetIfReified(self->msg);
  return msg ? hpb_Message_ExtensionCount(msg) : 0;
}

static PyObject* PyUpb_ExtensionDict_Subscript(PyObject* _self, PyObject* key) {
  PyUpb_ExtensionDict* self = (PyUpb_ExtensionDict*)_self;
  const hpb_FieldDef* f = PyUpb_Message_GetExtensionDef(self->msg, key);
  if (!f) return NULL;
  return PyUpb_Message_GetFieldValue(self->msg, f);
}

static int PyUpb_ExtensionDict_AssignSubscript(PyObject* _self, PyObject* key,
                                               PyObject* val) {
  PyUpb_ExtensionDict* self = (PyUpb_ExtensionDict*)_self;
  const hpb_FieldDef* f = PyUpb_Message_GetExtensionDef(self->msg, key);
  if (!f) return -1;
  if (val) {
    return PyUpb_Message_SetFieldValue(self->msg, f, val, PyExc_TypeError);
  } else {
    PyUpb_Message_DoClearField(self->msg, f);
    return 0;
  }
}

static PyObject* PyUpb_ExtensionIterator_New(PyObject* _ext_dict);

static PyMethodDef PyUpb_ExtensionDict_Methods[] = {
    {"_FindExtensionByName", PyUpb_ExtensionDict_FindExtensionByName, METH_O,
     "Finds an extension by name."},
    {"_FindExtensionByNumber", PyUpb_ExtensionDict_FindExtensionByNumber,
     METH_O, "Finds an extension by number."},
    {NULL, NULL},
};

static PyType_Slot PyUpb_ExtensionDict_Slots[] = {
    {Py_tp_dealloc, PyUpb_ExtensionDict_Dealloc},
    {Py_tp_methods, PyUpb_ExtensionDict_Methods},
    //{Py_tp_getset, PyUpb_ExtensionDict_Getters},
    //{Py_tp_hash, PyObject_HashNotImplemented},
    {Py_tp_richcompare, PyUpb_ExtensionDict_RichCompare},
    {Py_tp_iter, PyUpb_ExtensionIterator_New},
    {Py_sq_contains, PyUpb_ExtensionDict_Contains},
    {Py_sq_length, PyUpb_ExtensionDict_Length},
    {Py_mp_length, PyUpb_ExtensionDict_Length},
    {Py_mp_subscript, PyUpb_ExtensionDict_Subscript},
    {Py_mp_ass_subscript, PyUpb_ExtensionDict_AssignSubscript},
    {0, NULL}};

static PyType_Spec PyUpb_ExtensionDict_Spec = {
    PYUPB_MODULE_NAME ".ExtensionDict",  // tp_name
    sizeof(PyUpb_ExtensionDict),         // tp_basicsize
    0,                                   // tp_itemsize
    Py_TPFLAGS_DEFAULT,                  // tp_flags
    PyUpb_ExtensionDict_Slots,
};

// -----------------------------------------------------------------------------
// ExtensionIterator
// -----------------------------------------------------------------------------

typedef struct {
  PyObject_HEAD;
  PyObject* msg;
  size_t iter;
} PyUpb_ExtensionIterator;

static PyObject* PyUpb_ExtensionIterator_New(PyObject* _ext_dict) {
  PyUpb_ExtensionDict* ext_dict = (PyUpb_ExtensionDict*)_ext_dict;
  PyUpb_ModuleState* state = PyUpb_ModuleState_Get();
  PyUpb_ExtensionIterator* iter =
      (void*)PyType_GenericAlloc(state->extension_iterator_type, 0);
  if (!iter) return NULL;
  iter->msg = ext_dict->msg;
  iter->iter = kHpb_Message_Begin;
  Py_INCREF(iter->msg);
  return &iter->ob_base;
}

static void PyUpb_ExtensionIterator_Dealloc(void* _self) {
  PyUpb_ExtensionIterator* self = (PyUpb_ExtensionIterator*)_self;
  Py_DECREF(self->msg);
  PyUpb_Dealloc(_self);
}

PyObject* PyUpb_ExtensionIterator_IterNext(PyObject* _self) {
  PyUpb_ExtensionIterator* self = (PyUpb_ExtensionIterator*)_self;
  hpb_Message* msg = PyUpb_Message_GetIfReified(self->msg);
  if (!msg) return NULL;
  const hpb_MessageDef* m = PyUpb_Message_GetMsgdef(self->msg);
  const hpb_DefPool* symtab = hpb_FileDef_Pool(hpb_MessageDef_File(m));
  while (true) {
    const hpb_FieldDef* f;
    hpb_MessageValue val;
    if (!hpb_Message_Next(msg, m, symtab, &f, &val, &self->iter)) return NULL;
    if (hpb_FieldDef_IsExtension(f)) return PyUpb_FieldDescriptor_Get(f);
  }
}

static PyType_Slot PyUpb_ExtensionIterator_Slots[] = {
    {Py_tp_dealloc, PyUpb_ExtensionIterator_Dealloc},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, PyUpb_ExtensionIterator_IterNext},
    {0, NULL}};

static PyType_Spec PyUpb_ExtensionIterator_Spec = {
    PYUPB_MODULE_NAME ".ExtensionIterator",  // tp_name
    sizeof(PyUpb_ExtensionIterator),         // tp_basicsize
    0,                                       // tp_itemsize
    Py_TPFLAGS_DEFAULT,                      // tp_flags
    PyUpb_ExtensionIterator_Slots,
};

// -----------------------------------------------------------------------------
// Top Level
// -----------------------------------------------------------------------------

bool PyUpb_InitExtensionDict(PyObject* m) {
  PyUpb_ModuleState* s = PyUpb_ModuleState_GetFromModule(m);

  s->extension_dict_type = PyUpb_AddClass(m, &PyUpb_ExtensionDict_Spec);
  s->extension_iterator_type = PyUpb_AddClass(m, &PyUpb_ExtensionIterator_Spec);

  return s->extension_dict_type && s->extension_iterator_type;
}
