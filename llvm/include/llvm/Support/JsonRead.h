//===--- JsonRead.cpp - JSON Parser Class ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#ifndef TRANSFORMS_OBFUSCATION_JSONREAD_H
#define TRANSFORMS_OBFUSCATION_JSONREAD_H

#include <map>
#include <type_traits>
#include <vector>

#include "llvm/Support/JSON.h"

namespace llvm {

class JsonRead {
public:
  template <typename T> bool Parse(T &JsonObj, StringRef JsonStr);
  template <typename T> bool ParseImpl(T &JsonObj) {
    return JsonObj.json(this);
  }
#define OBJ_NO_IMPL_INSTANCE(T)                                                \
  template <> bool ParseImpl<T>(T & JsonObj) { llvm_unreachable("no impl"); }

  OBJ_NO_IMPL_INSTANCE(bool);
  OBJ_NO_IMPL_INSTANCE(int);
  OBJ_NO_IMPL_INSTANCE(float);
  OBJ_NO_IMPL_INSTANCE(char);
  OBJ_NO_IMPL_INSTANCE(std::string);
#undef OBJ_NO_IMPL_INSTANCE
  template <typename T> bool ParseImpl(std ::vector<T> &JsonObj) {
    llvm_unreachable("no impl");
  }

  template <typename T> bool ParseArrayNode(T &Obj) {
    llvm_unreachable("no impl");
  }
  template <typename T> bool ParseArrayNode(std::vector<T> &Obj);
#define ARR_NO_IMPL_INSTANCE(T)                                                \
  template <> bool ParseArrayNode<T>(T & Obj) { llvm_unreachable("no impl"); }

  ARR_NO_IMPL_INSTANCE(bool);
  ARR_NO_IMPL_INSTANCE(int);
  ARR_NO_IMPL_INSTANCE(float);
  ARR_NO_IMPL_INSTANCE(char);
  ARR_NO_IMPL_INSTANCE(std::string);
#undef ARR_NO_IMPL_INSTANCE

  template <typename T> bool ParseObjNode(const StringRef &Key, T &Obj);

private:
  void *CurrObjValue;
};

// vector type trait
template <typename T> struct is_vector : std::false_type {};
template <typename T> struct is_vector<std::vector<T>> : std::true_type {};

template <typename T> struct is_string : std::false_type {};
template <> struct is_string<std::string> : std::true_type {};

// Object type trait
template <typename T> struct obj_return_value {
  static bool get_return(json::Object *JsonObj, T &Obj, StringRef Key) {
    llvm_unreachable("no impl");
  }
};

template <> struct obj_return_value<int> {
  static bool get_return(json::Object *JsonObj, int &Obj, StringRef Key) {
    assert(JsonObj->getInteger(Key).hasValue() && "should have value");
    Obj = *JsonObj->getInteger(Key);
    return true;
  }
};

template <> struct obj_return_value<float> {
  static bool get_return(json::Object *JsonObj, float &Obj, StringRef Key) {
    assert(JsonObj->getNumber(Key).hasValue() && "should have value");
    Obj = JsonObj->getNumber(Key).getValue();
    return true;
  }
};

template <> struct obj_return_value<std::string> {
  static bool get_return(json::Object *Obj, std::string &RetObj,
                         StringRef Key) {
    assert(JsonObj->getString(Key).hasValue() && "should have value");
    RetObj = Obj->getString(Key).getValue().str();
    return true;
  }
};

template <> struct obj_return_value<bool> {
  static bool get_return(json::Object *Obj, bool &RetObj, StringRef Key) {
    assert(JsonObj->getBoolean(Key).hasValue() && "should have value");
    RetObj = Obj->getBoolean(Key).getValue();
    return true;
  }
};

// Array type trait
template <typename T> struct arr_return_value {
  static bool get_return(json::Value &Obj) { return false; }
};

template <> struct arr_return_value<int> {
  static bool get_return(json::Value &Obj, int &Ret) {
    assert(Obj.getAsNumber().hasValue() && "should have value");
    Ret = *Obj.getAsNumber();
    return true;
  }
};

template <> struct arr_return_value<float> {
  static bool get_return(json::Value &Obj, float &Ret) {
    assert(Obj.getAsNumber().hasValue() && "should have value");
    Ret = *Obj.getAsNumber();
    return true;
  }
};

template <> struct arr_return_value<std::string> {
  static bool get_return(json::Value &Obj, std::string &Ret) {
    Ret = Obj.getAsString()->str();
    return true;
  }
};

template <typename T> bool JsonRead::Parse(T &JsonObj, StringRef JsonStr) {
  auto TmpObjValue = json::parse(JsonStr);
  if (Error Err = TmpObjValue.takeError()) {
    report_fatal_error("failed to parser json file");
  }
  json::Value ObjValue = std::move(TmpObjValue.get());
  CurrObjValue = ObjValue.getAsObject();
  return ParseImpl(JsonObj);
}

template <typename T> bool JsonRead::ParseArrayNode(std::vector<T> &Obj) {
  using ValueTy = T;

  json::Array *Arr = (json::Array *)CurrObjValue;
  auto Begin = Arr->begin();
  auto End = Arr->end();
  for (; Begin != End; Begin++) {
    ValueTy Elem;
    json::Value ArrayValue = *Begin;
    json::Value::Kind JsonKind = ArrayValue.kind();

    switch (JsonKind) {
    case json::Value::Number:
    case json::Value::String:
      arr_return_value<ValueTy>::get_return(ArrayValue, Elem);
      break;
    case json::Value::Object:
      CurrObjValue = ArrayValue.getAsObject();
      ParseImpl(Elem);
      break;
    case json::Value::Array:
      CurrObjValue = ArrayValue.getAsArray();
      ParseArrayNode(Elem);
      break;
    default:
      llvm_unreachable("no impl");
    }
    Obj.push_back(Elem);
  }
  CurrObjValue = Arr;
  return true;
}

template <typename T>
bool JsonRead::ParseObjNode(const StringRef &Key, T &Obj) {
  json::Object *TmpObj = (json::Object *)CurrObjValue;
  bool ret = false;
  if (std::is_integral<T>::value || std::is_floating_point<T>::value ||
      is_string<T>::value) {
    return obj_return_value<T>::get_return(TmpObj, Obj, Key);
  } else if (is_vector<T>::value) {
    CurrObjValue = TmpObj->getArray(Key);
    ret = ParseArrayNode(Obj);
    CurrObjValue = TmpObj;
  } else {
    CurrObjValue = TmpObj->getObject(Key);
    ret = ParseImpl(Obj);
    CurrObjValue = TmpObj;
  }
  return ret;
}

} // namespace llvm

#endif
