// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/spec_reader.h"

#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/values.h>
#include <gtest/gtest.h>

#include "soma/container_spec_wrapper.h"
#include "soma/namespace.h"
#include "soma/port.h"

namespace soma {
namespace parser {

class ContainerSpecReaderTest : public ::testing::Test {
 public:
  ContainerSpecReaderTest() {}
  virtual ~ContainerSpecReaderTest() {}

  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(tmpdir_.path(), &scratch_));
  }

 protected:
  scoped_ptr<base::DictionaryValue> BuildBaselineValue() {
    scoped_ptr<base::DictionaryValue> baseline_spec(new base::DictionaryValue);
    baseline_spec->SetString(ContainerSpecReader::kServiceBundlePathKey,
                             kServiceBundlePath);
    baseline_spec->SetInteger(ContainerSpecReader::kUidKey, kUid);
    baseline_spec->SetInteger(ContainerSpecReader::kGidKey, kGid);
    return baseline_spec.Pass();
  }

  void WriteValue(base::Value* to_write, const base::FilePath& file) {
    std::string value_string;
    ASSERT_TRUE(base::JSONWriter::Write(to_write, &value_string));
    ASSERT_EQ(
        base::WriteFile(file, value_string.c_str(), value_string.length()),
        value_string.length());
  }

  void CheckSpecBaseline(ContainerSpecWrapper* spec) {
    ASSERT_TRUE(spec);
    ASSERT_EQ(spec->service_bundle_path(),
              base::FilePath(kServiceBundlePath));
    ASSERT_EQ(spec->uid(), kUid);
    ASSERT_EQ(spec->gid(), kGid);
  }

  ContainerSpecReader reader_;
  base::ScopedTempDir tmpdir_;
  base::FilePath scratch_;

 private:
  static const char kServiceBundlePath[];
  static const uid_t kUid;
  static const gid_t kGid;
};

const char ContainerSpecReaderTest::kServiceBundlePath[] = "/services/bundle";
const uid_t ContainerSpecReaderTest::kUid = 1;
const gid_t ContainerSpecReaderTest::kGid = 2;

TEST_F(ContainerSpecReaderTest, BaselineSpec) {
  scoped_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  WriteValue(baseline.get(), scratch_);

  scoped_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
}

TEST_F(ContainerSpecReaderTest, SpecWithListenPorts) {
  scoped_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  const parser::port::Number port1 = 80;
  const parser::port::Number port2 = 9222;
  const parser::port::Number invalid_port = -8;
  scoped_ptr<base::ListValue> listen_ports(new base::ListValue);
  listen_ports->AppendInteger(port1);
  listen_ports->AppendInteger(port2);
  listen_ports->AppendInteger(invalid_port);
  baseline->Set(parser::port::kListKey, listen_ports.release());

  WriteValue(baseline.get(), scratch_);

  scoped_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->ListenPortIsAllowed(port2));
  EXPECT_TRUE(spec->ListenPortIsAllowed(port1));
  EXPECT_FALSE(spec->ListenPortIsAllowed(81));
  EXPECT_FALSE(spec->ListenPortIsAllowed(invalid_port));
}

TEST_F(ContainerSpecReaderTest, SpecWithWildcardPort) {
  scoped_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  scoped_ptr<base::ListValue> listen_ports(new base::ListValue);
  listen_ports->AppendInteger(parser::port::kWildcard);
  baseline->Set(parser::port::kListKey, listen_ports.release());

  WriteValue(baseline.get(), scratch_);

  scoped_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->ListenPortIsAllowed(80));
  EXPECT_TRUE(spec->ListenPortIsAllowed(90));
}

namespace {
scoped_ptr<base::ListValue> ListFromPair(const std::pair<int, int>& pair) {
  scoped_ptr<base::ListValue> list(new base::ListValue);
  list->AppendInteger(pair.first);
  list->AppendInteger(pair.second);
  return list.Pass();
}
}  // anonymous namespace

TEST_F(ContainerSpecReaderTest, SpecWithDeviceFilters) {
  scoped_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  const char kPathFilter1[] = "/dev/d1";
  const char kPathFilter2[] = "/dev/d2";
  scoped_ptr<base::ListValue> device_path_filters(new base::ListValue);
  device_path_filters->AppendString(kPathFilter1);
  device_path_filters->AppendString(kPathFilter2);
  baseline->Set(DevicePathFilter::kListKey, device_path_filters.release());

  scoped_ptr<base::ListValue> device_node_filters(new base::ListValue);
  device_node_filters->Append(ListFromPair({8, 0}).release());
  device_node_filters->Append(ListFromPair({4, -1}).release());
  baseline->Set(DeviceNodeFilter::kListKey, device_node_filters.release());

  WriteValue(baseline.get(), scratch_);

  scoped_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->DevicePathIsAllowed(base::FilePath(kPathFilter1)));
  EXPECT_TRUE(spec->DevicePathIsAllowed(base::FilePath(kPathFilter2)));
  EXPECT_TRUE(spec->DeviceNodeIsAllowed(8, 0));
  EXPECT_TRUE(spec->DeviceNodeIsAllowed(4, -1));
}

TEST_F(ContainerSpecReaderTest, SpecWithNamespaces) {
  scoped_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  scoped_ptr<base::ListValue> namespaces(new base::ListValue);
  namespaces->AppendString(ns::kNewIpc);
  namespaces->AppendString(ns::kNewPid);
  baseline->Set(ns::kListKey, namespaces.release());

  WriteValue(baseline.get(), scratch_);

  scoped_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->ShouldApplyNamespace(ContainerSpec::NEWIPC));
  EXPECT_TRUE(spec->ShouldApplyNamespace(ContainerSpec::NEWPID));
  EXPECT_FALSE(spec->ShouldApplyNamespace(ContainerSpec::NEWNS));
}

}  // namespace parser
}  // namespace soma
