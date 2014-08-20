// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/method_name_generator.h"

#include <string>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "chromeos-dbus-bindings/interface.h"

using std::string;
using testing::Test;

namespace chromeos_dbus_bindings {

namespace {

const char kMethodName0[] = "Zircon";
const char kMethodName1[] = "Encrusted";
const char kMethodName2[] = "Tweezers";
const char kExpectedOutput[] =
    "const char kZirconMethod[] = \"Zircon\";\n"
    "const char kEncrustedMethod[] = \"Encrusted\";\n"
    "const char kTweezersMethod[] = \"Tweezers\";\n";
}  // namespace

class MethodNameGeneratorTest : public Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  base::FilePath CreateInputFile(const string& contents) {
    base::FilePath path;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &path));
    EXPECT_EQ(contents.size(),
              base::WriteFile(path, contents.c_str(), contents.size()));
    return path;
  }

  base::ScopedTempDir temp_dir_;
  MethodNameGenerator generator_;
};

TEST_F(MethodNameGeneratorTest, GnerateMethodNames) {
  Interface interface;
  interface.methods.emplace_back(kMethodName0);
  interface.methods.emplace_back(kMethodName1);
  interface.methods.emplace_back(kMethodName2);
  base::FilePath output_path = temp_dir_.path().Append("output.h");
  EXPECT_TRUE(generator_.GenerateMethodNames(interface, output_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  EXPECT_STREQ(kExpectedOutput, contents.c_str());
}

}  // namespace chromeos_dbus_bindings
