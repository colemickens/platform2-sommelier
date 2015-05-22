// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/soma.h"

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <gtest/gtest.h>
#include <protobinder/binder_manager_stub.h>

#include "soma/lib/soma/sandbox_spec_reader.h"
#include "soma/proto_bindings/soma.pb.h"
#include "soma/proto_bindings/soma_sandbox_spec.pb.h"

namespace soma {

class SomaTest : public ::testing::Test {
 public:
  SomaTest() {
    CHECK(tmpdir_.CreateUniqueTempDir());
    protobinder::BinderManagerInterface::SetForTesting(
        scoped_ptr<BinderManagerInterface>(new protobinder::BinderManagerStub));
  }
  ~SomaTest() override {
    protobinder::BinderManagerInterface::SetForTesting(
        scoped_ptr<BinderManagerInterface>());
  }

  void SetUp() override {
    SandboxSpec spec;
    std::string spec_string;
    ASSERT_TRUE(spec.SerializeToString(&spec_string));
    std::string file_name = kEndpointNamespace + std::string(".spec");
    ASSERT_EQ(base::WriteFile(tmpdir_.path().AppendASCII(file_name),
                              spec_string.c_str(), spec_string.length()),
              spec_string.length());
  }

 protected:
  static const char kEndpointNamespace[];
  static const char kEndpointName[];

  base::ScopedTempDir tmpdir_;
};

const char SomaTest::kEndpointNamespace[] = "com.android.embedded.ping-overlay";
const char SomaTest::kEndpointName[] = "ping-endpoint";

TEST_F(SomaTest, FindSpecProtoFile) {
  Soma soma(base::FilePath(tmpdir_.path()));
  GetSandboxSpecRequest request;
  GetSandboxSpecResponse response;
  request.set_endpoint_name(
      JoinString({kEndpointNamespace, kEndpointName}, '.'));

  EXPECT_TRUE(soma.GetSandboxSpec(&request, &response));
  EXPECT_TRUE(response.has_sandbox_spec());
}

TEST_F(SomaTest, SpecFileNotFound) {
  Soma soma(base::FilePath(tmpdir_.path()));
  GetSandboxSpecRequest request;
  GetSandboxSpecResponse response;
  request.set_endpoint_name(JoinString({"org.shut.up", kEndpointName}, '.'));

  EXPECT_TRUE(soma.GetSandboxSpec(&request, &response));
  EXPECT_FALSE(response.has_sandbox_spec());
}

TEST_F(SomaTest, MalformedRequest) {
  Soma soma(base::FilePath("."));
  GetSandboxSpecRequest request;
  GetSandboxSpecResponse response;
  EXPECT_FALSE(soma.GetSandboxSpec(&request, &response));

  request.set_endpoint_name(".");
  EXPECT_FALSE(soma.GetSandboxSpec(&request, &response));
  request.set_endpoint_name("..");
  EXPECT_FALSE(soma.GetSandboxSpec(&request, &response));
  request.set_endpoint_name("../../etc/passwd");
  EXPECT_FALSE(soma.GetSandboxSpec(&request, &response));
  request.set_endpoint_name("subdir/thing.json");
  EXPECT_FALSE(soma.GetSandboxSpec(&request, &response));
}

TEST_F(SomaTest, GetPersistentSandboxSpecs) {
  Soma soma(base::FilePath(tmpdir_.path()));

  GetPersistentSandboxSpecsRequest request;
  GetPersistentSandboxSpecsResponse response;
  EXPECT_TRUE(soma.GetPersistentSandboxSpecs(&request, &response));
  EXPECT_EQ(response.sandbox_specs_size(), 0);

  // Create a SandboxSpec that has a persistent tag on it and write it out
  // to a new file name in tmpdir_.
  std::string file_name = kEndpointNamespace + std::string("-2.spec");
  SandboxSpec spec;
  spec.set_is_persistent(true);
  spec.set_name(file_name);
  std::string spec_string;
  ASSERT_TRUE(spec.SerializeToString(&spec_string));
  ASSERT_EQ(base::WriteFile(tmpdir_.path().AppendASCII(file_name),
                            spec_string.c_str(), spec_string.length()),
            spec_string.length());

  response.Clear();
  EXPECT_TRUE(soma.GetPersistentSandboxSpecs(&request, &response));
  ASSERT_EQ(1, response.sandbox_specs_size());
  EXPECT_EQ(file_name, response.sandbox_specs(0).name());
}

}  // namespace soma
