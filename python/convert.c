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

#include "python/convert.h"

#include "python/message.h"
#include "python/protobuf.h"
#include "hpb/collections/map.h"
#include "hpb/reflection/message.h"
#include "hpb/util/compare.h"

// Must be last.
#include "hpb/port/def.inc"

PyObject* PyUpb_UpbToPy(hpb_MessageValue val, const hpb_FieldDef* f,
                        PyObject* arena) {
  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Enum:
    case kHpb_CType_Int32:
      return PyLong_FromLong(val.int32_val);
    case kHpb_CType_Int64:
      return PyLong_FromLongLong(val.int64_val);
    case kHpb_CType_UInt32:
      return PyLong_FromSize_t(val.uint32_val);
    case kHpb_CType_UInt64:
      return PyLong_FromUnsignedLongLong(val.uint64_val);
    case kHpb_CType_Float:
      return PyFloat_FromDouble(val.float_val);
    case kHpb_CType_Double:
      return PyFloat_FromDouble(val.double_val);
    case kHpb_CType_Bool:
      return PyBool_FromLong(val.bool_val);
    case kHpb_CType_Bytes:
      return PyBytes_FromStringAndSize(val.str_val.data, val.str_val.size);
    case kHpb_CType_String: {
      PyObject* ret =
          PyUnicode_DecodeUTF8(val.str_val.data, val.str_val.size, NULL);
      // If the string can't be decoded in UTF-8, just return a bytes object
      // that contains the raw bytes. This can't happen if the value was
      // assigned using the members of the Python message object, but can happen
      // if the values were parsed from the wire (binary).
      if (ret == NULL) {
        PyErr_Clear();
        ret = PyBytes_FromStringAndSize(val.str_val.data, val.str_val.size);
      }
      return ret;
    }
    case kHpb_CType_Message:
      return PyUpb_Message_Get((hpb_Message*)val.msg_val,
                               hpb_FieldDef_MessageSubDef(f), arena);
    default:
      PyErr_Format(PyExc_SystemError,
                   "Getting a value from a field of unknown type %d",
                   hpb_FieldDef_CType(f));
      return NULL;
  }
}

static bool PyUpb_GetInt64(PyObject* obj, int64_t* val) {
  // We require that the value is either an integer or has an __index__
  // conversion.
  obj = PyNumber_Index(obj);
  if (!obj) return false;
  // If the value is already a Python long, PyLong_AsLongLong() retrieves it.
  // Otherwise is converts to integer using __int__.
  *val = PyLong_AsLongLong(obj);
  bool ok = true;
  if (PyErr_Occurred()) {
    assert(PyErr_ExceptionMatches(PyExc_OverflowError));
    PyErr_Clear();
    PyErr_Format(PyExc_ValueError, "Value out of range: %S", obj);
    ok = false;
  }
  Py_DECREF(obj);
  return ok;
}

static bool PyUpb_GetUint64(PyObject* obj, uint64_t* val) {
  // We require that the value is either an integer or has an __index__
  // conversion.
  obj = PyNumber_Index(obj);
  if (!obj) return false;
  *val = PyLong_AsUnsignedLongLong(obj);
  bool ok = true;
  if (PyErr_Occurred()) {
    assert(PyErr_ExceptionMatches(PyExc_OverflowError));
    PyErr_Clear();
    PyErr_Format(PyExc_ValueError, "Value out of range: %S", obj);
    ok = false;
  }
  Py_DECREF(obj);
  return ok;
}

static bool PyUpb_GetInt32(PyObject* obj, int32_t* val) {
  int64_t i64;
  if (!PyUpb_GetInt64(obj, &i64)) return false;
  if (i64 < INT32_MIN || i64 > INT32_MAX) {
    PyErr_Format(PyExc_ValueError, "Value out of range: %S", obj);
    return false;
  }
  *val = i64;
  return true;
}

static bool PyUpb_GetUint32(PyObject* obj, uint32_t* val) {
  uint64_t u64;
  if (!PyUpb_GetUint64(obj, &u64)) return false;
  if (u64 > UINT32_MAX) {
    PyErr_Format(PyExc_ValueError, "Value out of range: %S", obj);
    return false;
  }
  *val = u64;
  return true;
}

// If `arena` is specified, copies the string data into the given arena.
// Otherwise aliases the given data.
static hpb_MessageValue PyUpb_MaybeCopyString(const char* ptr, size_t size,
                                              hpb_Arena* arena) {
  hpb_MessageValue ret;
  ret.str_val.size = size;
  if (arena) {
    char* buf = hpb_Arena_Malloc(arena, size);
    memcpy(buf, ptr, size);
    ret.str_val.data = buf;
  } else {
    ret.str_val.data = ptr;
  }
  return ret;
}

const char* upb_FieldDef_TypeString(const hpb_FieldDef* f) {
  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Double:
      return "double";
    case kHpb_CType_Float:
      return "float";
    case kHpb_CType_Int64:
      return "int64";
    case kHpb_CType_Int32:
      return "int32";
    case kHpb_CType_UInt64:
      return "uint64";
    case kHpb_CType_UInt32:
      return "uint32";
    case kHpb_CType_Enum:
      return "enum";
    case kHpb_CType_Bool:
      return "bool";
    case kHpb_CType_String:
      return "string";
    case kHpb_CType_Bytes:
      return "bytes";
    case kHpb_CType_Message:
      return "message";
  }
  HPB_UNREACHABLE();
}

static bool PyUpb_PyToUpbEnum(PyObject* obj, const hpb_EnumDef* e,
                              hpb_MessageValue* val) {
  if (PyUnicode_Check(obj)) {
    Py_ssize_t size;
    const char* name = PyUnicode_AsUTF8AndSize(obj, &size);
    const hpb_EnumValueDef* ev =
        hpb_EnumDef_FindValueByNameWithSize(e, name, size);
    if (!ev) {
      PyErr_Format(PyExc_ValueError, "unknown enum label \"%s\"", name);
      return false;
    }
    val->int32_val = hpb_EnumValueDef_Number(ev);
    return true;
  } else {
    int32_t i32;
    if (!PyUpb_GetInt32(obj, &i32)) return false;
    if (hpb_FileDef_Syntax(hpb_EnumDef_File(e)) == kHpb_Syntax_Proto2 &&
        !hpb_EnumDef_CheckNumber(e, i32)) {
      PyErr_Format(PyExc_ValueError, "invalid enumerator %d", (int)i32);
      return false;
    }
    val->int32_val = i32;
    return true;
  }
}

bool PyUpb_IsNumpyNdarray(PyObject* obj, const hpb_FieldDef* f) {
  PyObject* type_name_obj =
      PyObject_GetAttrString((PyObject*)Py_TYPE(obj), "__name__");
  bool is_ndarray = false;
  if (!strcmp(PyUpb_GetStrData(type_name_obj), "ndarray")) {
    PyErr_Format(PyExc_TypeError,
                 "%S has type ndarray, but expected one of: %s", obj,
                 upb_FieldDef_TypeString(f));
    is_ndarray = true;
  }
  Py_DECREF(type_name_obj);
  return is_ndarray;
}

bool PyUpb_PyToUpb(PyObject* obj, const hpb_FieldDef* f, hpb_MessageValue* val,
                   hpb_Arena* arena) {
  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Enum:
      return PyUpb_PyToUpbEnum(obj, hpb_FieldDef_EnumSubDef(f), val);
    case kHpb_CType_Int32:
      return PyUpb_GetInt32(obj, &val->int32_val);
    case kHpb_CType_Int64:
      return PyUpb_GetInt64(obj, &val->int64_val);
    case kHpb_CType_UInt32:
      return PyUpb_GetUint32(obj, &val->uint32_val);
    case kHpb_CType_UInt64:
      return PyUpb_GetUint64(obj, &val->uint64_val);
    case kHpb_CType_Float:
      if (PyUpb_IsNumpyNdarray(obj, f)) return false;
      val->float_val = PyFloat_AsDouble(obj);
      return !PyErr_Occurred();
    case kHpb_CType_Double:
      if (PyUpb_IsNumpyNdarray(obj, f)) return false;
      val->double_val = PyFloat_AsDouble(obj);
      return !PyErr_Occurred();
    case kHpb_CType_Bool:
      if (PyUpb_IsNumpyNdarray(obj, f)) return false;
      val->bool_val = PyLong_AsLong(obj);
      return !PyErr_Occurred();
    case kHpb_CType_Bytes: {
      char* ptr;
      Py_ssize_t size;
      if (PyBytes_AsStringAndSize(obj, &ptr, &size) < 0) return false;
      *val = PyUpb_MaybeCopyString(ptr, size, arena);
      return true;
    }
    case kHpb_CType_String: {
      Py_ssize_t size;
      const char* ptr;
      PyObject* unicode = NULL;
      if (PyBytes_Check(obj)) {
        unicode = obj = PyUnicode_FromEncodedObject(obj, "utf-8", NULL);
        if (!obj) return false;
      }
      ptr = PyUnicode_AsUTF8AndSize(obj, &size);
      if (PyErr_Occurred()) {
        Py_XDECREF(unicode);
        return false;
      }
      *val = PyUpb_MaybeCopyString(ptr, size, arena);
      Py_XDECREF(unicode);
      return true;
    }
    case kHpb_CType_Message:
      PyErr_Format(PyExc_ValueError, "Message objects may not be assigned");
      return false;
    default:
      PyErr_Format(PyExc_SystemError,
                   "Getting a value from a field of unknown type %d",
                   hpb_FieldDef_CType(f));
      return false;
  }
}

bool upb_Message_IsEqual(const hpb_Message* msg1, const hpb_Message* msg2,
                         const hpb_MessageDef* m);

// -----------------------------------------------------------------------------
// Equal
// -----------------------------------------------------------------------------

bool PyUpb_ValueEq(hpb_MessageValue val1, hpb_MessageValue val2,
                   const hpb_FieldDef* f) {
  switch (hpb_FieldDef_CType(f)) {
    case kHpb_CType_Bool:
      return val1.bool_val == val2.bool_val;
    case kHpb_CType_Int32:
    case kHpb_CType_UInt32:
    case kHpb_CType_Enum:
      return val1.int32_val == val2.int32_val;
    case kHpb_CType_Int64:
    case kHpb_CType_UInt64:
      return val1.int64_val == val2.int64_val;
    case kHpb_CType_Float:
      return val1.float_val == val2.float_val;
    case kHpb_CType_Double:
      return val1.double_val == val2.double_val;
    case kHpb_CType_String:
    case kHpb_CType_Bytes:
      return val1.str_val.size == val2.str_val.size &&
             memcmp(val1.str_val.data, val2.str_val.data, val1.str_val.size) ==
                 0;
    case kHpb_CType_Message:
      return upb_Message_IsEqual(val1.msg_val, val2.msg_val,
                                 hpb_FieldDef_MessageSubDef(f));
    default:
      return false;
  }
}

bool PyUpb_Map_IsEqual(const hpb_Map* map1, const hpb_Map* map2,
                       const hpb_FieldDef* f) {
  assert(hpb_FieldDef_IsMap(f));
  if (map1 == map2) return true;

  size_t size1 = map1 ? hpb_Map_Size(map1) : 0;
  size_t size2 = map2 ? hpb_Map_Size(map2) : 0;
  if (size1 != size2) return false;
  if (size1 == 0) return true;

  const hpb_MessageDef* entry_m = hpb_FieldDef_MessageSubDef(f);
  const hpb_FieldDef* val_f = hpb_MessageDef_Field(entry_m, 1);
  size_t iter = kHpb_Map_Begin;

  hpb_MessageValue key, val1;
  while (hpb_Map_Next(map1, &key, &val1, &iter)) {
    hpb_MessageValue val2;
    if (!hpb_Map_Get(map2, key, &val2)) return false;
    if (!PyUpb_ValueEq(val1, val2, val_f)) return false;
  }

  return true;
}

static bool PyUpb_ArrayElem_IsEqual(const hpb_Array* arr1,
                                    const hpb_Array* arr2, size_t i,
                                    const hpb_FieldDef* f) {
  assert(i < hpb_Array_Size(arr1));
  assert(i < hpb_Array_Size(arr2));
  hpb_MessageValue val1 = hpb_Array_Get(arr1, i);
  hpb_MessageValue val2 = hpb_Array_Get(arr2, i);
  return PyUpb_ValueEq(val1, val2, f);
}

bool PyUpb_Array_IsEqual(const hpb_Array* arr1, const hpb_Array* arr2,
                         const hpb_FieldDef* f) {
  assert(hpb_FieldDef_IsRepeated(f) && !hpb_FieldDef_IsMap(f));
  if (arr1 == arr2) return true;

  size_t n1 = arr1 ? hpb_Array_Size(arr1) : 0;
  size_t n2 = arr2 ? hpb_Array_Size(arr2) : 0;
  if (n1 != n2) return false;

  // Half the length rounded down.  Important: the empty list rounds to 0.
  size_t half = n1 / 2;

  // Search from the ends-in.  We expect differences to more quickly manifest
  // at the ends than in the middle.  If the length is odd we will miss the
  // middle element.
  for (size_t i = 0; i < half; i++) {
    if (!PyUpb_ArrayElem_IsEqual(arr1, arr2, i, f)) return false;
    if (!PyUpb_ArrayElem_IsEqual(arr1, arr2, n1 - 1 - i, f)) return false;
  }

  // For an odd-lengthed list, pick up the middle element.
  if (n1 & 1) {
    if (!PyUpb_ArrayElem_IsEqual(arr1, arr2, half, f)) return false;
  }

  return true;
}

bool upb_Message_IsEqual(const hpb_Message* msg1, const hpb_Message* msg2,
                         const hpb_MessageDef* m) {
  if (msg1 == msg2) return true;
  if (hpb_Message_ExtensionCount(msg1) != hpb_Message_ExtensionCount(msg2))
    return false;

  // Compare messages field-by-field.  This is slightly tricky, because while
  // we can iterate over normal fields in a predictable order, the extension
  // order is unpredictable and may be different between msg1 and msg2.
  // So we use the following strategy:
  //   1. Iterate over all msg1 fields (including extensions).
  //   2. For non-extension fields, we find the corresponding field by simply
  //      using hpb_Message_Next(msg2).  If the two messages have the same set
  //      of fields, this will yield the same field.
  //   3. For extension fields, we have to actually search for the corresponding
  //      field, which we do with hpb_Message_GetFieldByDef(msg2, ext_f1).
  //   4. Once iteration over msg1 is complete, we call hpb_Message_Next(msg2)
  //   one
  //      final time to verify that we have visited all of msg2's regular fields
  //      (we pass NULL for ext_dict so that iteration will *not* return
  //      extensions).
  //
  // We don't need to visit all of msg2's extensions, because we verified up
  // front that both messages have the same number of extensions.
  const hpb_DefPool* symtab = hpb_FileDef_Pool(hpb_MessageDef_File(m));
  const hpb_FieldDef *f1, *f2;
  hpb_MessageValue val1, val2;
  size_t iter1 = kHpb_Message_Begin;
  size_t iter2 = kHpb_Message_Begin;
  while (hpb_Message_Next(msg1, m, symtab, &f1, &val1, &iter1)) {
    if (hpb_FieldDef_IsExtension(f1)) {
      val2 = hpb_Message_GetFieldByDef(msg2, f1);
    } else {
      if (!hpb_Message_Next(msg2, m, NULL, &f2, &val2, &iter2) || f1 != f2) {
        return false;
      }
    }

    if (hpb_FieldDef_IsMap(f1)) {
      if (!PyUpb_Map_IsEqual(val1.map_val, val2.map_val, f1)) return false;
    } else if (hpb_FieldDef_IsRepeated(f1)) {
      if (!PyUpb_Array_IsEqual(val1.array_val, val2.array_val, f1)) {
        return false;
      }
    } else {
      if (!PyUpb_ValueEq(val1, val2, f1)) return false;
    }
  }

  if (hpb_Message_Next(msg2, m, NULL, &f2, &val2, &iter2)) return false;

  size_t usize1, usize2;
  const char* uf1 = hpb_Message_GetUnknown(msg1, &usize1);
  const char* uf2 = hpb_Message_GetUnknown(msg2, &usize2);
  // 100 is arbitrary, we're trying to prevent stack overflow but it's not
  // obvious how deep we should allow here.
  return hpb_Message_UnknownFieldsAreEqual(uf1, usize1, uf2, usize2, 100) ==
         kHpb_UnknownCompareResult_Equal;
}

#include "hpb/port/undef.inc"
