//===-- JSONTest.cpp - JSON unit tests --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/JSON.h"
#include "llvm/Support/JsonObj.h"
#include "llvm/Support/JsonRead.h"
#include "llvm/Support/raw_ostream.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace llvm {

namespace {

struct TestJson : public JsonClass {
  bool json(JsonRead *JsonReadObj) {
    JsonReadObj->ParseObjNode("arr", arrays);
    JsonReadObj->ParseObjNode("str", str);
    JsonReadObj->ParseObjNode("int", intvalue);
    return true;
  }

  std::vector<int> arrays;
  std::string str;
  int intvalue;
};

TEST(JSONTest2, Simple) {
  std::string JsonStr = R"json(
{
    "arr" : [1, 2, 3, 4, 5, 6, 7],
    "str" : "hello world",
    "int" : 123
}
)json";
  JsonRead Json;
  TestJson TestObj;
  bool ret = Json.Parse(TestObj, JsonStr);

  ASSERT_TRUE(ret);
  ASSERT_EQ("hello world", TestObj.str);
  ASSERT_EQ(123, TestObj.intvalue);
  ASSERT_EQ((uint64_t)7, TestObj.arrays.size());

  ASSERT_EQ(1, TestObj.arrays[0]);
  ASSERT_EQ(2, TestObj.arrays[1]);
  ASSERT_EQ(3, TestObj.arrays[2]);
  ASSERT_EQ(4, TestObj.arrays[3]);
  ASSERT_EQ(5, TestObj.arrays[4]);
  ASSERT_EQ(6, TestObj.arrays[5]);
  ASSERT_EQ(7, TestObj.arrays[6]);
}

struct ConfigParams : public JsonClass {
  bool json(JsonRead *JsonReadObj) {
    JsonReadObj->ParseObjNode("only_procted_mark_fn", OnlyProtectedMarkFn);
    JsonReadObj->ParseObjNode("protected_fn_list", ProtectedFnList);
    JsonReadObj->ParseObjNode("protected_ratio", ProtectedRatio);
    return true;
  }

  bool OnlyProtectedMarkFn = false;
  std::vector<std::string> ProtectedFnList;
  float ProtectedRatio;
};

struct Config : public JsonClass {
  bool json(JsonRead *JsonReadObj) {
    JsonReadObj->ParseObjNode("config-name", ConfigName);
    JsonReadObj->ParseObjNode("params", Params);
    return true;
  }

  std::string ConfigName;
  ConfigParams Params;
};

TEST(JSONTest2, Complex) {
  std::string JsonStr = R"json(
{
    "config-name" : "TT_config",
    "params" :  {
        "only_procted_mark_fn": true,
        "protected_fn_list" : [
            "xxxxx",
            "yyyyy",
            "zzzzz"
        ],
        "protected_ratio" : 0.1
    }
}
)json";
  JsonRead Json;
  Config TestObj;
  bool ret = Json.Parse(TestObj, JsonStr);
  ASSERT_TRUE(ret);

  std::cout << TestObj.ConfigName;
}

} // namespace
} // namespace llvm
