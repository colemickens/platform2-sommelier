// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/soma.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/strings/string_util.h>
#include <base/values.h>
#include <gtest/gtest.h>

#include "soma/lib/soma/annotations.h"
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

TEST_F(SomaTest, FindSpecFile) {
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

TEST_F(SomaTest, SpecFileNotFound) {
  Soma soma(base::FilePath(tmpdir_.path()));
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  request.set_service_name(JoinString({kServiceNamespace, kServiceName}, '.'));

  EXPECT_EQ(soma.GetContainerSpec(&request, &response), 0);
  EXPECT_FALSE(response.has_container_spec());
}

TEST_F(SomaTest, MalformedRequest) {
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

TEST_F(SomaTest, GetContainerSpecs) {
  base::FilePath service_json =
      base::FilePath(kServiceNamespace).AddExtension("json");
  base::FilePath scratch_json(tmpdir_.path().Append(service_json));
  ASSERT_TRUE(base::CopyFile(service_json, scratch_json));

  Soma soma(base::FilePath(tmpdir_.path()));
  GetPersistentContainerSpecsRequest request;
  GetPersistentContainerSpecsResponse response;
  EXPECT_EQ(soma.GetPersistentContainerSpecs(&request, &response), 0);
  EXPECT_EQ(response.container_specs_size(), 0);

  // Read in the scratch spec and add the "persistent" annotation.
  base::JSONReader reader(base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  std::string json;
  ASSERT_TRUE(base::ReadFileToString(scratch_json, &json));

  std::unique_ptr<base::Value> root(reader.ReadToValue(json));
  base::DictionaryValue* spec_dict = nullptr;
  ASSERT_TRUE(!!root);
  ASSERT_TRUE(root->GetAsDictionary(&spec_dict));
  ASSERT_TRUE(parser::annotations::AddPersistentAnnotationForTest(spec_dict));

  // Now write it out to a new file name in the same directory.
  base::FilePath json2 = scratch_json.InsertBeforeExtension("-2");
  std::string value_string;
  ASSERT_TRUE(base::JSONWriter::Write(root.get(), &value_string));
  ASSERT_EQ(base::WriteFile(json2, value_string.c_str(), value_string.length()),
            value_string.length());

  response.Clear();
  EXPECT_EQ(soma.GetPersistentContainerSpecs(&request, &response), 0);
  ASSERT_EQ(response.container_specs_size(), 1);
  EXPECT_EQ(response.container_specs(0).name(), json2.value());
}

}  // namespace soma
