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

#include "soma/lib/soma/container_spec_reader.h"
#include "soma/proto_bindings/soma.pb.h"
#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {
using soma::parser::ContainerSpecReader;

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
    ContainerSpec spec;
    std::string spec_string;
    ASSERT_TRUE(spec.SerializeToString(&spec_string));
    std::string file_name = kServiceNamespace + std::string(".spec");
    ASSERT_EQ(base::WriteFile(tmpdir_.path().AppendASCII(file_name),
                              spec_string.c_str(), spec_string.length()),
              spec_string.length());
  }

 protected:
  static const char kServiceNamespace[];
  static const char kServiceName[];

  base::ScopedTempDir tmpdir_;
};

const char SomaTest::kServiceNamespace[] = "com.android.embedded.ping-brick";
const char SomaTest::kServiceName[] = "ping-service";

TEST_F(SomaTest, FindSpecProtoFile) {
  Soma soma(base::FilePath(tmpdir_.path()));
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  request.set_service_name(JoinString({kServiceNamespace, kServiceName}, '.'));

  EXPECT_TRUE(soma.GetContainerSpec(&request, &response));
  EXPECT_TRUE(response.has_container_spec());
}

TEST_F(SomaTest, SpecFileNotFound) {
  Soma soma(base::FilePath(tmpdir_.path()));
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  request.set_service_name(JoinString({"org.shut.up", kServiceName}, '.'));

  EXPECT_TRUE(soma.GetContainerSpec(&request, &response));
  EXPECT_FALSE(response.has_container_spec());
}

TEST_F(SomaTest, MalformedRequest) {
  Soma soma(base::FilePath("."));
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  EXPECT_FALSE(soma.GetContainerSpec(&request, &response));

  request.set_service_name(".");
  EXPECT_FALSE(soma.GetContainerSpec(&request, &response));
  request.set_service_name("..");
  EXPECT_FALSE(soma.GetContainerSpec(&request, &response));
  request.set_service_name("../../etc/passwd");
  EXPECT_FALSE(soma.GetContainerSpec(&request, &response));
  request.set_service_name("subdir/thing.json");
  EXPECT_FALSE(soma.GetContainerSpec(&request, &response));
}

TEST_F(SomaTest, GetContainerSpecs) {
  Soma soma(base::FilePath(tmpdir_.path()));

  GetPersistentContainerSpecsRequest request;
  GetPersistentContainerSpecsResponse response;
  EXPECT_TRUE(soma.GetPersistentContainerSpecs(&request, &response));
  EXPECT_EQ(response.container_specs_size(), 0);

  // Create a ContainerSpec that has a persistent tag on it and write it out
  // to a new file name in tmpdir_.
  std::string file_name = kServiceNamespace + std::string("-2.spec");
  ContainerSpec spec;
  spec.set_is_persistent(true);
  spec.set_name(file_name);
  std::string spec_string;
  ASSERT_TRUE(spec.SerializeToString(&spec_string));
  ASSERT_EQ(base::WriteFile(tmpdir_.path().AppendASCII(file_name),
                            spec_string.c_str(), spec_string.length()),
            spec_string.length());

  response.Clear();
  EXPECT_TRUE(soma.GetPersistentContainerSpecs(&request, &response));
  ASSERT_EQ(1, response.container_specs_size());
  EXPECT_EQ(file_name, response.container_specs(0).name());
}

}  // namespace soma
