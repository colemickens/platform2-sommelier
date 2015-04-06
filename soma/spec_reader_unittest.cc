// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/spec_reader.h"

#include <memory>
#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <gtest/gtest.h>

#include "soma/container_spec_wrapper.h"
#include "soma/namespace.h"
#include "soma/port.h"
#include "soma/service_name.h"

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
  std::unique_ptr<base::DictionaryValue> BuildBaselineValue() {
    return BuildBaselineWithCL(true);
  }

  std::unique_ptr<base::DictionaryValue> BuildBaselineValueNoCL() {
    return BuildBaselineWithCL(false);
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
    ASSERT_EQ(scratch_.value(), spec->name());
    ASSERT_EQ(base::UintToString(spec->uid()), kUid);
    ASSERT_EQ(base::UintToString(spec->gid()), kGid);

    ASSERT_PRED2(
        [](const std::string& a, const std::string& b) {
          return EndsWith(a, b, false /* case insensitive */);
        },
        spec->service_bundle_path().value(), kServiceBundleName);
  }

  ContainerSpecReader reader_;
  base::ScopedTempDir tmpdir_;
  base::FilePath scratch_;

 private:
  static const char kServiceBundleName[];
  static const char kUid[];
  static const char kGid[];

  std::unique_ptr<base::DictionaryValue> BuildBaselineWithCL(bool with_cl) {
    std::unique_ptr<base::DictionaryValue> app_dict(new base::DictionaryValue);
    app_dict->SetString(ContainerSpecReader::kServiceBundleNameKey,
                        kServiceBundleName);
    app_dict->SetString(ContainerSpecReader::kUidKey, kUid);
    app_dict->SetString(ContainerSpecReader::kGidKey, kGid);
    if (with_cl) {
      std::unique_ptr<base::ListValue> cl(new base::ListValue);
      cl->AppendString("foo");
      app_dict->Set(ContainerSpecReader::kCommandLineKey, cl.release());
    }
    std::unique_ptr<base::ListValue> apps_list(new base::ListValue);
    apps_list->Append(app_dict.release());
    std::unique_ptr<base::DictionaryValue> baseline(new base::DictionaryValue);
    baseline->Set(ContainerSpecReader::kAppsKey, apps_list.release());

    return std::move(baseline);
  }
};

const char ContainerSpecReaderTest::kServiceBundleName[] = "bundle";
const char ContainerSpecReaderTest::kUid[] = "1";
const char ContainerSpecReaderTest::kGid[] = "2";

TEST_F(ContainerSpecReaderTest, BaselineSpec) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
}

TEST_F(ContainerSpecReaderTest, EmptyCommandLine) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValueNoCL();
  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  EXPECT_EQ(spec.get(), nullptr);
}

namespace {
std::unique_ptr<base::DictionaryValue> CreateAnnotation(
    const std::string& name, const std::string& value) {
  std::unique_ptr<base::DictionaryValue> annotation(new base::DictionaryValue);
  annotation->SetString("name", name);
  annotation->SetString("value", value);
  return std::move(annotation);
}
}  // namespace

TEST_F(ContainerSpecReaderTest, OneServiceName) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  std::unique_ptr<base::ListValue> annotations(new base::ListValue);
  annotations->Append(CreateAnnotation("service-0", "foo").release());
  baseline->Set(parser::service_name::kListKey, annotations.release());

  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  EXPECT_TRUE(spec->ProvidesServiceNamed("foo"));
}

TEST_F(ContainerSpecReaderTest, SkipBogusServiceName) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  std::unique_ptr<base::ListValue> annotations(new base::ListValue);
  annotations->Append(CreateAnnotation("service-0", "foo").release());
  annotations->Append(CreateAnnotation("bugagoo", "bar").release());
  annotations->Append(CreateAnnotation("service-1", "baz").release());
  baseline->Set(parser::service_name::kListKey, annotations.release());

  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  EXPECT_TRUE(spec->ProvidesServiceNamed("foo"));
  EXPECT_TRUE(spec->ProvidesServiceNamed("baz"));
  EXPECT_FALSE(spec->ProvidesServiceNamed("bar"));
}

namespace {
std::unique_ptr<base::DictionaryValue> CreatePort(const std::string& protocol,
                                             const parser::port::Number port) {
  std::unique_ptr<base::DictionaryValue> port_dict(new base::DictionaryValue);
  port_dict->SetString(parser::port::kProtocolKey, protocol);
  port_dict->SetInteger(parser::port::kPortKey, port);
  return std::move(port_dict);
}
}  // namespace

TEST_F(ContainerSpecReaderTest, SpecWithListenPorts) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  const parser::port::Number port1 = 80;
  const parser::port::Number port2 = 9222;
  const parser::port::Number invalid_port = -8;
  std::unique_ptr<base::ListValue> listen_ports(new base::ListValue);
  listen_ports->Append(CreatePort(parser::port::kTcpProtocol, port1).release());
  listen_ports->Append(CreatePort(parser::port::kTcpProtocol, port2).release());
  listen_ports->Append(CreatePort(parser::port::kUdpProtocol, port1).release());
  baseline->Set(parser::port::kListKey, listen_ports.release());

  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->TcpListenPortIsAllowed(port1));
  EXPECT_TRUE(spec->TcpListenPortIsAllowed(port2));
  EXPECT_TRUE(spec->UdpListenPortIsAllowed(port1));
  EXPECT_FALSE(spec->UdpListenPortIsAllowed(81));

  std::unique_ptr<base::ListValue> listen_ports_invalid(new base::ListValue);
  listen_ports_invalid->Append(
      CreatePort(parser::port::kUdpProtocol, invalid_port).release());
  baseline->Set(parser::port::kListKey, listen_ports_invalid.release());

  WriteValue(baseline.get(), scratch_);
  spec = reader_.Read(scratch_);
  EXPECT_EQ(spec.get(), nullptr);
}

TEST_F(ContainerSpecReaderTest, SpecWithWildcardPort) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  std::unique_ptr<base::ListValue> listen_ports(new base::ListValue);
  listen_ports->Append(CreatePort(parser::port::kTcpProtocol,
                                  parser::port::kWildcard).release());
  baseline->Set(parser::port::kListKey, listen_ports.release());

  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->TcpListenPortIsAllowed(80));
  EXPECT_TRUE(spec->TcpListenPortIsAllowed(90));
  EXPECT_FALSE(spec->UdpListenPortIsAllowed(90));
}

namespace {
std::unique_ptr<base::ListValue> ListFromPair(const std::pair<int, int>& pair) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  list->AppendInteger(pair.first);
  list->AppendInteger(pair.second);
  return std::move(list);
}
}  // anonymous namespace

TEST_F(ContainerSpecReaderTest, SpecWithDeviceFilters) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  const char kPathFilter1[] = "/dev/d1";
  const char kPathFilter2[] = "/dev/d2";
  std::unique_ptr<base::ListValue> device_path_filters(new base::ListValue);
  device_path_filters->AppendString(kPathFilter1);
  device_path_filters->AppendString(kPathFilter2);
  baseline->Set(DevicePathFilter::kListKey, device_path_filters.release());

  std::unique_ptr<base::ListValue> device_node_filters(new base::ListValue);
  device_node_filters->Append(ListFromPair({8, 0}).release());
  device_node_filters->Append(ListFromPair({4, -1}).release());
  baseline->Set(DeviceNodeFilter::kListKey, device_node_filters.release());

  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->DevicePathIsAllowed(base::FilePath(kPathFilter1)));
  EXPECT_TRUE(spec->DevicePathIsAllowed(base::FilePath(kPathFilter2)));
  EXPECT_TRUE(spec->DeviceNodeIsAllowed(8, 0));
  EXPECT_TRUE(spec->DeviceNodeIsAllowed(4, -1));
}

TEST_F(ContainerSpecReaderTest, SpecWithNamespaces) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  std::unique_ptr<base::ListValue> namespaces(new base::ListValue);
  namespaces->AppendString(ns::kNewIpc);
  namespaces->AppendString(ns::kNewPid);
  baseline->Set(ns::kListKey, namespaces.release());

  WriteValue(baseline.get(), scratch_);

  std::unique_ptr<ContainerSpecWrapper> spec = reader_.Read(scratch_);
  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->ShouldApplyNamespace(ContainerSpec::NEWIPC));
  EXPECT_TRUE(spec->ShouldApplyNamespace(ContainerSpec::NEWPID));
  EXPECT_FALSE(spec->ShouldApplyNamespace(ContainerSpec::NEWNS));
}

}  // namespace parser
}  // namespace soma
