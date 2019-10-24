// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/vm_util.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace vm_tools {
namespace concierge {

TEST(VMUtilTest, LoadCustomParametersSupportsEmptyInput) {
  base::StringPairs args;
  LoadCustomParameters("", &args);
  base::StringPairs expected;
  EXPECT_THAT(args, testing::ContainerEq(expected));
}

TEST(VMUtilTest, LoadCustomParametersParsesManyPairs) {
  base::StringPairs args;
  LoadCustomParameters("Key1=Value1\nKey2=Value2\nKey3=Value3", &args);
  base::StringPairs expected = {
      {"Key1", "Value1"}, {"Key2", "Value2"}, {"Key3", "Value3"}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
}

TEST(VMUtilTest, LoadCustomParametersSkipsComments) {
  base::StringPairs args;
  LoadCustomParameters("Key1=Value1\n#Key2=Value2\nKey3=Value3", &args);
  base::StringPairs expected{{"Key1", "Value1"}, {"Key3", "Value3"}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
}

TEST(VMUtilTest, LoadCustomParametersSkipsEmptyLines) {
  base::StringPairs args;
  LoadCustomParameters("Key1=Value1\n\n\n\n\n\n\nKey2=Value2\n\n\n\n", &args);
  base::StringPairs expected{{"Key1", "Value1"}, {"Key2", "Value2"}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
}

TEST(VMUtilTest, LoadCustomParametersSupportsKeyWithoutValue) {
  base::StringPairs args;
  LoadCustomParameters("Key1=Value1\nKey2\n\n\n\nKey3", &args);
  base::StringPairs expected{{"Key1", "Value1"}, {"Key2", ""}, {"Key3", ""}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
}

TEST(VMUtilTest, LoadCustomParametersSupportsRemoving) {
  base::StringPairs args = {{"KeyToBeReplaced", "OldValue"},
                            {"KeyToBeKept", "ValueToBeKept"}};
  LoadCustomParameters(
      "Key1=Value1\nKey2=Value2\n!KeyToBeReplaced\nKeyToBeReplaced=NewValue",
      &args);
  base::StringPairs expected{{"KeyToBeKept", "ValueToBeKept"},
                             {"Key1", "Value1"},
                             {"Key2", "Value2"},
                             {"KeyToBeReplaced", "NewValue"}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
}

TEST(VMUtilTest, LoadCustomParametersSupportsRemovingByPrefix) {
  base::StringPairs args = {{"foo", ""},
                            {"foo", "bar"},
                            {"foobar", ""},
                            {"foobar", "baz"},
                            {"barfoo", ""}};
  LoadCustomParameters("!foo", &args);
  base::StringPairs expected{{"barfoo", ""}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
}

TEST(VMUtilTest, RemoveParametersWithKeyReturnsFoundValue) {
  base::StringPairs args = {{"KERNEL_PATH", "/a/b/c"}, {"Key1", "Value1"}};
  LoadCustomParameters("Key2=Value2\nKey3=Value3", &args);
  const std::string resolved_kernel_path =
      RemoveParametersWithKey("KERNEL_PATH", "default_path", &args);

  base::StringPairs expected{
      {"Key1", "Value1"}, {"Key2", "Value2"}, {"Key3", "Value3"}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
  EXPECT_THAT(resolved_kernel_path, "/a/b/c");
}

TEST(VMUtilTest, RemoveParametersWithKeyReturnsDefaultValue) {
  base::StringPairs args = {{"SOME_OTHER_PATH", "/a/b/c"}, {"Key1", "Value1"}};
  LoadCustomParameters("Key2=Value2\nKey3=Value3", &args);
  const std::string resolved_kernel_path =
      RemoveParametersWithKey("KERNEL_PATH", "default_path", &args);

  base::StringPairs expected{{"SOME_OTHER_PATH", "/a/b/c"},
                             {"Key1", "Value1"},
                             {"Key2", "Value2"},
                             {"Key3", "Value3"}};
  EXPECT_THAT(args, testing::ContainerEq(expected));
  EXPECT_THAT(resolved_kernel_path, "default_path");
}

}  // namespace concierge
}  // namespace vm_tools
