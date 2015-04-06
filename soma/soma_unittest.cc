// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/soma.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <gtest/gtest.h>

#include "soma/proto_bindings/soma.pb.h"

namespace soma {

class SomaTest : public ::testing::Test {
 public:
  SomaTest() = default;
  virtual ~SomaTest() = default;

  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
  }

 protected:
  static const char kServiceNamespace[];
  static const char kServiceName[];

  base::ScopedTempDir tmpdir_;
};

const char SomaTest::kServiceNamespace[] = "com.android.embedded.ping-brick";
const char SomaTest::kServiceName[] = "ping-service";

TEST_F(SomaTest, FindSpecFileTest) {
  base::FilePath service_json =
      base::FilePath(kServiceNamespace).AddExtension("json");
  base::FilePath scratch_json(tmpdir_.path().Append(service_json));
  ASSERT_TRUE(base::CopyFile(service_json, scratch_json));

  Soma soma(base::FilePath(tmpdir_.path()));
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  request.set_service_name(JoinString({kServiceNamespace, kServiceName}, '.'));

  EXPECT_EQ(soma.GetContainerSpec(&request, &response), 0);
  EXPECT_TRUE(response.has_container_spec());
}

TEST_F(SomaTest, SpecFileNotFoundTest) {
  Soma soma(base::FilePath(tmpdir_.path()));
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  request.set_service_name(JoinString({kServiceNamespace, kServiceName}, '.'));

  EXPECT_EQ(soma.GetContainerSpec(&request, &response), 0);
  EXPECT_FALSE(response.has_container_spec());
}

TEST_F(SomaTest, MalformedRequestTest) {
  Soma soma(base::FilePath("."));
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  EXPECT_NE(soma.GetContainerSpec(&request, &response), 0);

  request.set_service_name(".");
  EXPECT_NE(soma.GetContainerSpec(&request, &response), 0);
  request.set_service_name("..");
  EXPECT_NE(soma.GetContainerSpec(&request, &response), 0);
  request.set_service_name("../../etc/passwd");
  EXPECT_NE(soma.GetContainerSpec(&request, &response), 0);
  request.set_service_name("subdir/thing.json");
  EXPECT_NE(soma.GetContainerSpec(&request, &response), 0);
}

}  // namespace soma
