// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/soma.h"

#include <memory>
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
#include "soma/lib/soma/container_spec_reader.h"
#include "soma/lib/soma/fake_userdb.h"
#include "soma/proto_bindings/soma.pb.h"

namespace soma {
using soma::parser::ContainerSpecReader;

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

  // Creates a ContainerSpecReader that uses a Userdb that has uid and gid
  // mappings for every user and group in the given namespace.
  std::unique_ptr<ContainerSpecReader> CreateReaderWithWhitelistedNamespace(
      const std::string& whitelisted_namespace) {
    std::unique_ptr<ContainerSpecReader> reader;
    std::unique_ptr<parser::FakeUserdb> fakedb(new parser::FakeUserdb);
    fakedb->set_user_mapping("chronos", 1000);
    fakedb->set_group_mapping("chronos", 1001);
    fakedb->set_whitelisted_namespace(whitelisted_namespace);
    reader.reset(new ContainerSpecReader(std::move(fakedb)));
    return std::move(reader);
  }

  void InjectReader(Soma* soma, std::unique_ptr<ContainerSpecReader> reader) {
    soma->InjectReader(std::move(reader));
  }

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
  InjectReader(&soma, CreateReaderWithWhitelistedNamespace(kServiceNamespace));
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
  request.set_service_name(JoinString({kServiceNamespace, kServiceName}, '.'));

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
  base::FilePath service_json =
      base::FilePath(kServiceNamespace).AddExtension("json");
  base::FilePath scratch_json(tmpdir_.path().Append(service_json));
  ASSERT_TRUE(base::CopyFile(service_json, scratch_json));

  Soma soma(base::FilePath(tmpdir_.path()));
  InjectReader(&soma, CreateReaderWithWhitelistedNamespace(kServiceNamespace));
  GetPersistentContainerSpecsRequest request;
  GetPersistentContainerSpecsResponse response;
  EXPECT_TRUE(soma.GetPersistentContainerSpecs(&request, &response));
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
  EXPECT_TRUE(soma.GetPersistentContainerSpecs(&request, &response));
  ASSERT_EQ(response.container_specs_size(), 1);
  EXPECT_EQ(response.container_specs(0).name(), json2.value());
}

}  // namespace soma
