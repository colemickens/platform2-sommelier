// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/sandbox_spec_reader.h"

#include <sys/types.h>

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "soma/proto_bindings/soma_sandbox_spec.pb.h"

namespace soma {

class SandboxSpecReaderTest : public ::testing::Test {
 public:
  SandboxSpecReaderTest() = default;
  virtual ~SandboxSpecReaderTest() = default;

  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(tmpdir_.path(), &scratch_));
  }

 protected:
  base::FilePath scratch_;
  base::ScopedTempDir tmpdir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxSpecReaderTest);
};

TEST_F(SandboxSpecReaderTest, FileNotFound) {
  SandboxSpecReader reader;
  EXPECT_EQ(nullptr, reader.Read(tmpdir_.path().AppendASCII("foo")));
}

TEST_F(SandboxSpecReaderTest, SpecFound) {
  const char expected_name[] = "com.foo.heythere";
  {
    SandboxSpec spec;
    spec.set_name(expected_name);
    std::string serialized;
    ASSERT_TRUE(spec.SerializeToString(&serialized));
    ASSERT_EQ(
        base::WriteFile(scratch_, serialized.c_str(), serialized.length()),
        serialized.length());
  }
  SandboxSpecReader reader;
  std::unique_ptr<SandboxSpec> read_spec = reader.Read(scratch_);
  EXPECT_EQ(expected_name, read_spec->name());
}

}  // namespace soma
