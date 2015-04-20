// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/container_spec_reader.h"

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
#include <base/strings/stringprintf.h>
#include <base/values.h>
#include <gtest/gtest.h>

#include "soma/lib/soma/annotations.h"
#include "soma/lib/soma/device_filter.h"
#include "soma/lib/soma/namespace.h"
#include "soma/lib/soma/port.h"
#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {
namespace parser {

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

class ContainerSpecWrapper {
 public:
  explicit ContainerSpecWrapper(std::unique_ptr<ContainerSpec> to_wrap)
      : internal_(std::move(to_wrap)) {
  }
  virtual ~ContainerSpecWrapper() = default;

  const std::string& name() const { return internal_->name(); }
  base::FilePath service_bundle_path() const {
    return base::FilePath(internal_->service_bundle_path());
  }
  uid_t uid_of_executable(int index) const {
    return internal_->executables(index).uid();
  }
  gid_t gid_of_executable(int index) const {
    return internal_->executables(index).gid();
  }

  bool is_persistent() const { return internal_->is_persistent(); }

  bool ProvidesServiceNamed(const std::string& name) const {
    const RepeatedPtrField<std::string>& names = internal_->service_names();
    return std::find(names.begin(), names.end(), name) != names.end();
  }

  bool ShouldApplyNamespace(ns::Kind candidate) const {
    const RepeatedField<int>& namespaces = internal_->namespaces();
    return std::find(namespaces.begin(), namespaces.end(), candidate) !=
        namespaces.end();
  }

  bool TcpListenPortIsAllowedForExecutable(port::Number port, int index) const {
    return ListenPortIsAllowed(internal_->executables(index).tcp_listen_ports(),
                               port);
  }

  bool UdpListenPortIsAllowedForExecutable(port::Number port, int index) const {
    return ListenPortIsAllowed(internal_->executables(index).udp_listen_ports(),
                               port);
  }

  bool DevicePathIsAllowed(const base::FilePath& query) const {
    const RepeatedPtrField<ContainerSpec::DevicePathFilter>& filters =
        internal_->device_path_filters();
    return
        filters.end() !=
        std::find_if(filters.begin(), filters.end(),
                     [query](const ContainerSpec::DevicePathFilter& to_check) {
                       DevicePathFilter parser_filter(
                           base::FilePath(to_check.filter()));
                       return parser_filter.Allows(query);
                     });
  }

  bool DeviceNodeIsAllowed(int major, int minor) const {
    const RepeatedPtrField<ContainerSpec::DeviceNodeFilter>& filters =
        internal_->device_node_filters();
    return
        filters.end() !=
        std::find_if(
            filters.begin(), filters.end(),
            [major, minor](const ContainerSpec::DeviceNodeFilter& to_check) {
              DeviceNodeFilter parser_filter(to_check.major(),
                                             to_check.minor());
              return parser_filter.Allows(major, minor);
            });
  }

 private:
  bool ListenPortIsAllowed(const ContainerSpec::PortSpec& port_spec,
                           port::Number port) const {
    if (port_spec.allow_all())
      return true;
    const RepeatedField<uint32>& ports = port_spec.ports();
    return std::find(ports.begin(), ports.end(), port) != ports.end();
  }

  std::unique_ptr<ContainerSpec> internal_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpecWrapper);
};

class ContainerSpecReaderTest : public ::testing::Test {
 public:
  ContainerSpecReaderTest() = default;
  virtual ~ContainerSpecReaderTest() = default;

  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(tmpdir_.path(), &scratch_));
  }

 protected:
  std::unique_ptr<base::DictionaryValue> BuildBaselineValue() {
    return BuildBaselineWithCommandLine(true);
  }

  std::unique_ptr<base::DictionaryValue> BuildBaselineValueNoCL() {
    return BuildBaselineWithCommandLine(false);
  }

  std::unique_ptr<base::DictionaryValue> BuildAppDict(const std::string& uid,
                                                      const std::string& gid,
                                                      const std::string& cmd) {
    std::unique_ptr<base::DictionaryValue> app_dict(new base::DictionaryValue);
    app_dict->SetString(ContainerSpecReader::kServiceBundleNameKey,
                        kServiceBundleName);

    std::string uid_key = MakeSubAppKey(ContainerSpecReader::kUidKey);
    std::string gid_key = MakeSubAppKey(ContainerSpecReader::kGidKey);
    std::string cl_key = MakeSubAppKey(ContainerSpecReader::kCommandLineKey);
    app_dict->SetString(uid_key, uid);
    app_dict->SetString(gid_key, gid);
    if (!cmd.empty()) {
      std::unique_ptr<base::ListValue> cl(new base::ListValue);
      cl->AppendString(cmd);
      app_dict->Set(cl_key, cl.release());
    }
    return std::move(app_dict);
  }

  void CheckSpecBaseline(ContainerSpecWrapper* spec) {
    ASSERT_TRUE(spec);
    ASSERT_EQ(scratch_.value(), spec->name());
    ASSERT_EQ(base::UintToString(spec->uid_of_executable(0)), kUid);
    ASSERT_EQ(base::UintToString(spec->gid_of_executable(0)), kGid);
    ASSERT_FALSE(spec->is_persistent());

    ASSERT_PRED2(
        [](const std::string& a, const std::string& b) {
          return EndsWith(a, b, false /* case insensitive */);
        },
        spec->service_bundle_path().value(), kServiceBundleName);
  }

  std::unique_ptr<ContainerSpecWrapper> ValueToSpec(base::Value* in) {
    WriteValue(in, scratch_);
    std::unique_ptr<ContainerSpec> spec = reader_.Read(scratch_);
    if (spec) {
      return std::unique_ptr<ContainerSpecWrapper>(
          new ContainerSpecWrapper(std::move(spec)));
    }
    return nullptr;
  }

  base::DictionaryValue* GetAppDict(base::DictionaryValue* pod_dict,
                                    int index) {
    base::ListValue* apps_list = nullptr;
    base::DictionaryValue* app_dict = nullptr;
    CHECK(pod_dict->GetList(ContainerSpecReader::kAppsListKey, &apps_list));
    CHECK(apps_list->GetDictionary(index, &app_dict));
    return app_dict;
  }

  std::string MakeSubAppKey(const std::string& element) {
    return JoinString({ContainerSpecReader::kSubAppKey, element}, '.');
  }

  ContainerSpecReader reader_;
  base::ScopedTempDir tmpdir_;

 private:
  static const char kServiceBundleName[];
  static const char kUid[];
  static const char kGid[];

  std::unique_ptr<base::DictionaryValue> BuildBaselineWithCommandLine(
      bool with_command_line) {
    std::string command;
    if (with_command_line) {
      command = "/bin/true";
    }
    std::unique_ptr<base::ListValue> apps_list(new base::ListValue);
    apps_list->Append(BuildAppDict(kUid, kGid, command).release());
    std::unique_ptr<base::DictionaryValue> baseline(new base::DictionaryValue);
    baseline->Set(ContainerSpecReader::kAppsListKey, apps_list.release());

    return std::move(baseline);
  }

  void WriteValue(base::Value* to_write, const base::FilePath& file) {
    std::string value_string;
    ASSERT_TRUE(base::JSONWriter::Write(to_write, &value_string));
    ASSERT_EQ(
        base::WriteFile(file, value_string.c_str(), value_string.length()),
        value_string.length());
  }

  base::FilePath scratch_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpecReaderTest);
};

const char ContainerSpecReaderTest::kServiceBundleName[] = "bundle";
const char ContainerSpecReaderTest::kUid[] = "1";
const char ContainerSpecReaderTest::kGid[] = "2";

TEST_F(ContainerSpecReaderTest, BaselineSpec) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());
  CheckSpecBaseline(spec.get());
}

TEST_F(ContainerSpecReaderTest, EmptyCommandLine) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValueNoCL();
  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());
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

std::unique_ptr<base::ListValue> CreateAnnotationList(
    const std::string& name, const std::string& value) {
  std::unique_ptr<base::ListValue> annotations(new base::ListValue);
  annotations->Append(CreateAnnotation(name, value).release());
  return std::move(annotations);
}
}  // namespace

TEST_F(ContainerSpecReaderTest, Persistent) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  baseline->Set(
      annotations::kListKey,
      CreateAnnotationList(annotations::kPersistentKey, "true").release());

  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  EXPECT_TRUE(spec->is_persistent());
}

TEST_F(ContainerSpecReaderTest, NotPersistent) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  baseline->Set(
      annotations::kListKey,
      CreateAnnotationList(annotations::kPersistentKey, "not").release());

  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  EXPECT_FALSE(spec->is_persistent());
}

TEST_F(ContainerSpecReaderTest, OneServiceName) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  baseline->Set(
      annotations::kListKey,
      CreateAnnotationList(annotations::MakeServiceNameKey(0), "z").release());

  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  EXPECT_TRUE(spec->ProvidesServiceNamed("z"));
}

TEST_F(ContainerSpecReaderTest, SkipBogusServiceName) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  std::unique_ptr<base::ListValue> annotations(new base::ListValue);
  annotations->Append(
      CreateAnnotation(annotations::MakeServiceNameKey(0), "foo").release());
  annotations->Append(CreateAnnotation("bugagoo", "bar").release());
  annotations->Append(
      CreateAnnotation(annotations::MakeServiceNameKey(1), "baz").release());
  baseline->Set(annotations::kListKey, annotations.release());

  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  EXPECT_TRUE(spec->ProvidesServiceNamed("foo"));
  EXPECT_TRUE(spec->ProvidesServiceNamed("baz"));
  EXPECT_FALSE(spec->ProvidesServiceNamed("bar"));
}

namespace {
std::unique_ptr<base::DictionaryValue> CreatePort(const std::string& protocol,
                                                  const port::Number port) {
  std::unique_ptr<base::DictionaryValue> port_dict(new base::DictionaryValue);
  port_dict->SetString(port::kProtocolKey, protocol);
  port_dict->SetInteger(port::kPortKey, port);
  return std::move(port_dict);
}
}  // namespace

TEST_F(ContainerSpecReaderTest, SpecWithListenPorts) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  const port::Number port1 = 80;
  const port::Number port2 = 4000;
  const port::Number port3 = 9222;
  {
    std::unique_ptr<base::ListValue> listen_ports(new base::ListValue);
    listen_ports->Append(CreatePort(port::kTcpProtocol, port1).release());
    listen_ports->Append(CreatePort(port::kTcpProtocol, port2).release());
    listen_ports->Append(CreatePort(port::kUdpProtocol, port1).release());
    GetAppDict(baseline.get(), 0)->Set(MakeSubAppKey(port::kListKey),
                                       listen_ports.release());
  }
  {
    std::unique_ptr<base::DictionaryValue> app2 =
        BuildAppDict("0", "0", "/bin/false");
    std::unique_ptr<base::ListValue> listen_ports(new base::ListValue);
    listen_ports->Append(CreatePort(port::kUdpProtocol, port3).release());
    app2->Set(MakeSubAppKey(port::kListKey), listen_ports.release());

    base::ListValue* apps = nullptr;
    ASSERT_TRUE(baseline->GetList(ContainerSpecReader::kAppsListKey, &apps));
    apps->Append(app2.release());
  }
  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->TcpListenPortIsAllowedForExecutable(port1, 0));
  EXPECT_TRUE(spec->TcpListenPortIsAllowedForExecutable(port2, 0));
  EXPECT_TRUE(spec->UdpListenPortIsAllowedForExecutable(port1, 0));
  EXPECT_FALSE(spec->UdpListenPortIsAllowedForExecutable(port3, 0));
  EXPECT_TRUE(spec->UdpListenPortIsAllowedForExecutable(port3, 1));
  EXPECT_FALSE(spec->UdpListenPortIsAllowedForExecutable(port1, 1));
  EXPECT_FALSE(spec->TcpListenPortIsAllowedForExecutable(port1, 1));
  {
    std::unique_ptr<base::ListValue> listen_ports_invalid(new base::ListValue);
    listen_ports_invalid->Append(CreatePort(port::kUdpProtocol, -8).release());
    GetAppDict(baseline.get(), 0)->Set(MakeSubAppKey(port::kListKey),
                                    listen_ports_invalid.release());
  }
  spec = ValueToSpec(baseline.get());
  EXPECT_EQ(spec.get(), nullptr);
}

TEST_F(ContainerSpecReaderTest, SpecWithWildcardPort) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();

  std::unique_ptr<base::ListValue> listen_ports(new base::ListValue);
  listen_ports->Append(CreatePort(port::kTcpProtocol,
                                  port::kWildcard).release());
  GetAppDict(baseline.get(), 0)->Set(MakeSubAppKey(port::kListKey),
                                     listen_ports.release());

  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->TcpListenPortIsAllowedForExecutable(80, 0));
  EXPECT_TRUE(spec->TcpListenPortIsAllowedForExecutable(90, 0));
  EXPECT_FALSE(spec->UdpListenPortIsAllowedForExecutable(90, 0));
}

namespace {
std::string IsolatorSetKey() {
  return base::StringPrintf("%s.%s", IsolatorParserInterface::kValueKey,
                            IsolatorSetParser::kKey);
}

std::unique_ptr<base::DictionaryValue> MakeIsolatorSet(
    const std::string& name,
    std::unique_ptr<base::ListValue> set) {
  std::unique_ptr<base::DictionaryValue> isolator(new base::DictionaryValue);
  isolator->SetString(IsolatorParserInterface::kNameKey, name);
  isolator->Set(IsolatorSetKey(), set.release());
  return std::move(isolator);
}

std::unique_ptr<base::DictionaryValue> FilterFromPair(
    const std::pair<int, int>& pair) {
  std::unique_ptr<base::DictionaryValue> filter(new base::DictionaryValue);
  filter->SetInteger("major", pair.first);
  filter->SetInteger("minor", pair.second);
  return std::move(filter);
}
}  // anonymous namespace

TEST_F(ContainerSpecReaderTest, SpecWithDeviceFilters) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  std::unique_ptr<base::ListValue> isolators(new base::ListValue);

  const char kPathFilter1[] = "/dev/d1";
  const char kPathFilter2[] = "/dev/d2";
  std::unique_ptr<base::ListValue> device_path_filters(new base::ListValue);
  device_path_filters->AppendString(kPathFilter1);
  device_path_filters->AppendString(kPathFilter2);
  isolators->Append(MakeIsolatorSet(DevicePathFilterParser::kName,
                                    std::move(device_path_filters)).release());

  std::unique_ptr<base::ListValue> device_node_filters(new base::ListValue);
  device_node_filters->Append(FilterFromPair({8, 0}).release());
  device_node_filters->Append(FilterFromPair({4, -1}).release());
  isolators->Append(MakeIsolatorSet(DeviceNodeFilterParser::kName,
                                    std::move(device_node_filters)).release());
  baseline->Set(ContainerSpecReader::kIsolatorsListKey, isolators.release());

  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  CheckSpecBaseline(spec.get());
  EXPECT_TRUE(spec->DevicePathIsAllowed(base::FilePath(kPathFilter1)));
  EXPECT_TRUE(spec->DevicePathIsAllowed(base::FilePath(kPathFilter2)));
  EXPECT_TRUE(spec->DeviceNodeIsAllowed(8, 0));
  EXPECT_TRUE(spec->DeviceNodeIsAllowed(4, -1));
}

TEST_F(ContainerSpecReaderTest, SpecWithNamespaces) {
  std::unique_ptr<base::DictionaryValue> baseline = BuildBaselineValue();
  std::unique_ptr<base::ListValue> isolators(new base::ListValue);

  std::unique_ptr<base::ListValue> namespaces(new base::ListValue);
  namespaces->AppendString(ns::kNewIpc);
  namespaces->AppendString(ns::kNewPid);
  isolators->Append(MakeIsolatorSet(NamespacesParser::kName,
                                    std::move(namespaces)).release());
  baseline->Set(ContainerSpecReader::kIsolatorsListKey, isolators.release());

  std::unique_ptr<ContainerSpecWrapper> spec = ValueToSpec(baseline.get());

  CheckSpecBaseline(spec.get());
  EXPECT_FALSE(spec->ShouldApplyNamespace(ContainerSpec::NEWIPC));
  EXPECT_FALSE(spec->ShouldApplyNamespace(ContainerSpec::NEWPID));
  EXPECT_TRUE(spec->ShouldApplyNamespace(ContainerSpec::NEWNS));
}

}  // namespace parser
}  // namespace soma
