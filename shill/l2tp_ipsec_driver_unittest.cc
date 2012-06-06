// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/l2tp_ipsec_driver.h"

#include <base/file_util.h>
#include <base/scoped_temp_dir.h>
#include <base/string_util.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_device_info.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_nss.h"
#include "shill/mock_process_killer.h"
#include "shill/mock_vpn.h"
#include "shill/mock_vpn_service.h"
#include "shill/property_store_inspector.h"
#include "shill/vpn.h"

using std::find;
using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::ElementsAreArray;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace shill {

class L2TPIPSecDriverTest : public testing::Test,
                            public RPCTaskDelegate {
 public:
  L2TPIPSecDriverTest()
      : device_info_(&control_, &dispatcher_, &metrics_, &manager_),
        manager_(&control_, &dispatcher_, &metrics_, &glib_),
        driver_(new L2TPIPSecDriver(&control_, &dispatcher_, &metrics_,
                                    &manager_, &device_info_, &glib_)),
        service_(new MockVPNService(&control_, &dispatcher_, &metrics_,
                                    &manager_, driver_)),
        device_(new MockVPN(&control_, &dispatcher_, &metrics_, &manager_,
                            kInterfaceName, kInterfaceIndex)) {
    driver_->nss_ = &nss_;
    driver_->process_killer_ = &process_killer_;
  }

  virtual ~L2TPIPSecDriverTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
    driver_->child_watch_tag_ = 0;
    driver_->pid_ = 0;
    driver_->device_ = NULL;
    driver_->service_ = NULL;
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  static const char kInterfaceName[];
  static const int kInterfaceIndex;

  void SetArg(const string &arg, const string &value) {
    driver_->args()->SetString(arg, value);
  }

  KeyValueStore *GetArgs() {
    return driver_->args();
  }

  // Used to assert that a flag appears in the options.
  void ExpectInFlags(const vector<string> &options, const string &flag,
                     const string &value);

  FilePath SetupPSKFile();

  // Inherited from RPCTaskDelegate.
  virtual void GetLogin(string *user, string *password);
  virtual void Notify(const string &reason, const map<string, string> &dict);

  ScopedTempDir temp_dir_;
  NiceMockControl control_;
  NiceMock<MockDeviceInfo> device_info_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  L2TPIPSecDriver *driver_;  // Owned by |service_|.
  scoped_refptr<MockVPNService> service_;
  scoped_refptr<MockVPN> device_;
  MockNSS nss_;
  MockProcessKiller process_killer_;
};

const char L2TPIPSecDriverTest::kInterfaceName[] = "ppp0";
const int L2TPIPSecDriverTest::kInterfaceIndex = 123;

void L2TPIPSecDriverTest::GetLogin(string */*user*/, string */*password*/) {}

void L2TPIPSecDriverTest::Notify(
    const string &/*reason*/, const map<string, string> &/*dict*/) {}

void L2TPIPSecDriverTest::ExpectInFlags(
    const vector<string> &options, const string &flag, const string &value) {
  vector<string>::const_iterator it =
      find(options.begin(), options.end(), flag);

  EXPECT_TRUE(it != options.end());
  if (it != options.end())
    return;  // Don't crash below.
  it++;
  EXPECT_TRUE(it != options.end());
  if (it != options.end())
    return;  // Don't crash below.
  EXPECT_EQ(value, *it);
}

FilePath L2TPIPSecDriverTest::SetupPSKFile() {
  FilePath psk_file;
  EXPECT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(), &psk_file));
  EXPECT_FALSE(psk_file.empty());
  EXPECT_TRUE(file_util::PathExists(psk_file));
  driver_->psk_file_ = psk_file;
  return psk_file;
}

TEST_F(L2TPIPSecDriverTest, GetProviderType) {
  EXPECT_EQ(flimflam::kProviderL2tpIpsec, driver_->GetProviderType());
}

TEST_F(L2TPIPSecDriverTest, Cleanup) {
  driver_->Cleanup(Service::kStateIdle);  // Ensure no crash.

  const unsigned int kTag = 123;
  driver_->child_watch_tag_ = kTag;
  EXPECT_CALL(glib_, SourceRemove(kTag));
  const int kPID = 123456;
  driver_->pid_ = kPID;
  EXPECT_CALL(process_killer_, Kill(kPID, _));
  driver_->device_ = device_;
  driver_->service_ = service_;
  EXPECT_CALL(*device_, OnDisconnected());
  EXPECT_CALL(*device_, SetEnabled(false));
  EXPECT_CALL(*service_, SetState(Service::kStateFailure));
  driver_->rpc_task_.reset(new RPCTask(&control_, this));
  FilePath psk_file = SetupPSKFile();
  driver_->StartConnectTimeout();
  driver_->Cleanup(Service::kStateFailure);
  EXPECT_FALSE(file_util::PathExists(psk_file));
  EXPECT_TRUE(driver_->psk_file_.empty());
  EXPECT_EQ(0, driver_->child_watch_tag_);
  EXPECT_EQ(0, driver_->pid_);
  EXPECT_FALSE(driver_->rpc_task_.get());
  EXPECT_FALSE(driver_->device_);
  EXPECT_FALSE(driver_->service_);
  EXPECT_FALSE(driver_->IsConnectTimeoutStarted());
}

TEST_F(L2TPIPSecDriverTest, DeletePSKFile) {
  FilePath psk_file = SetupPSKFile();
  driver_->DeletePSKFile();
  EXPECT_FALSE(file_util::PathExists(psk_file));
  EXPECT_TRUE(driver_->psk_file_.empty());
}

TEST_F(L2TPIPSecDriverTest, InitEnvironment) {
  vector<string> env;
  driver_->rpc_task_.reset(new RPCTask(&control_, this));
  driver_->InitEnvironment(&env);
  ASSERT_EQ(3, env.size());
  EXPECT_EQ(string("CONNMAN_BUSNAME=") + RPCTaskMockAdaptor::kRpcConnId,
            env[0]);
  EXPECT_EQ(string("CONNMAN_INTERFACE=") + RPCTaskMockAdaptor::kRpcInterfaceId,
            env[1]);
  EXPECT_EQ(string("CONNMAN_PATH=") + RPCTaskMockAdaptor::kRpcId, env[2]);
}

TEST_F(L2TPIPSecDriverTest, InitOptionsNoHost) {
  Error error;
  vector<string> options;
  EXPECT_FALSE(driver_->InitOptions(&options, &error));
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_TRUE(options.empty());
}

TEST_F(L2TPIPSecDriverTest, InitOptions) {
  static const char kHost[] = "192.168.2.254";
  static const char kCaCertNSS[] = "{1234}";
  static const char kPSK[] = "foobar";

  SetArg(flimflam::kProviderHostProperty, kHost);
  SetArg(flimflam::kL2tpIpsecCaCertNssProperty, kCaCertNSS);
  SetArg(flimflam::kL2tpIpsecPskProperty, kPSK);

  FilePath empty_cert;
  EXPECT_CALL(nss_, GetDERCertfile(kCaCertNSS, _)).WillOnce(Return(empty_cert));

  const FilePath temp_dir(temp_dir_.path());
  EXPECT_CALL(manager_, run_path()).WillOnce(ReturnRef(temp_dir));

  Error error;
  vector<string> options;
  EXPECT_TRUE(driver_->InitOptions(&options, &error));
  EXPECT_TRUE(error.IsSuccess());

  ExpectInFlags(options, "--remote_host", kHost);
  ASSERT_FALSE(driver_->psk_file_.empty());
  ExpectInFlags(options, "--psk_file", driver_->psk_file_.value());
}

TEST_F(L2TPIPSecDriverTest, InitPSKOptions) {
  Error error;
  vector<string> options;
  static const char kPSK[] = "foobar";
  const FilePath bad_dir("/non/existent/directory");
  const FilePath temp_dir(temp_dir_.path());
  EXPECT_CALL(manager_, run_path())
      .WillOnce(ReturnRef(bad_dir))
      .WillOnce(ReturnRef(temp_dir));

  EXPECT_TRUE(driver_->InitPSKOptions(&options, &error));
  EXPECT_TRUE(options.empty());
  EXPECT_TRUE(error.IsSuccess());

  SetArg(flimflam::kL2tpIpsecPskProperty, kPSK);

  EXPECT_FALSE(driver_->InitPSKOptions(&options, &error));
  EXPECT_TRUE(options.empty());
  EXPECT_EQ(Error::kInternalError, error.type());
  error.Reset();

  EXPECT_TRUE(driver_->InitPSKOptions(&options, &error));
  ASSERT_FALSE(driver_->psk_file_.empty());
  ExpectInFlags(options, "--psk_file", driver_->psk_file_.value());
  EXPECT_TRUE(error.IsSuccess());
  string contents;
  EXPECT_TRUE(
      file_util::ReadFileToString(driver_->psk_file_, &contents));
  EXPECT_EQ(kPSK, contents);
  struct stat buf;
  ASSERT_EQ(0, stat(driver_->psk_file_.value().c_str(), &buf));
  EXPECT_EQ(S_IFREG | S_IRUSR | S_IWUSR, buf.st_mode);
}

TEST_F(L2TPIPSecDriverTest, InitNSSOptions) {
  static const char kHost[] = "192.168.2.254";
  static const char kCaCertNSS[] = "{1234}";
  static const char kNSSCertfile[] = "/tmp/nss-cert";
  FilePath empty_cert;
  FilePath nss_cert(kNSSCertfile);
  SetArg(flimflam::kProviderHostProperty, kHost);
  SetArg(flimflam::kL2tpIpsecCaCertNssProperty, kCaCertNSS);
  EXPECT_CALL(nss_,
              GetDERCertfile(kCaCertNSS,
                             ElementsAreArray(kHost, arraysize(kHost) - 1)))
      .WillOnce(Return(empty_cert))
      .WillOnce(Return(nss_cert));

  vector<string> options;
  driver_->InitNSSOptions(&options);
  EXPECT_TRUE(options.empty());
  driver_->InitNSSOptions(&options);
  ExpectInFlags(options, "--server_ca_file", kNSSCertfile);
}

TEST_F(L2TPIPSecDriverTest, AppendValueOption) {
  static const char kOption[] = "--l2tpipsec-option";
  static const char kProperty[] = "L2TPIPSec.SomeProperty";
  static const char kValue[] = "some-property-value";
  static const char kOption2[] = "--l2tpipsec-option2";
  static const char kProperty2[] = "L2TPIPSec.SomeProperty2";
  static const char kValue2[] = "some-property-value2";

  vector<string> options;
  EXPECT_FALSE(
      driver_->AppendValueOption(
          "L2TPIPSec.UnknownProperty", kOption, &options));
  EXPECT_TRUE(options.empty());

  SetArg(kProperty, "");
  EXPECT_FALSE(driver_->AppendValueOption(kProperty, kOption, &options));
  EXPECT_TRUE(options.empty());

  SetArg(kProperty, kValue);
  SetArg(kProperty2, kValue2);
  EXPECT_TRUE(driver_->AppendValueOption(kProperty, kOption, &options));
  EXPECT_TRUE(driver_->AppendValueOption(kProperty2, kOption2, &options));
  EXPECT_EQ(4, options.size());
  EXPECT_EQ(kOption, options[0]);
  EXPECT_EQ(kValue, options[1]);
  EXPECT_EQ(kOption2, options[2]);
  EXPECT_EQ(kValue2, options[3]);
}

TEST_F(L2TPIPSecDriverTest, AppendFlag) {
  static const char kTrueOption[] = "--l2tpipsec-option";
  static const char kFalseOption[] = "--nol2tpipsec-option";
  static const char kProperty[] = "L2TPIPSec.SomeProperty";
  static const char kTrueOption2[] = "--l2tpipsec-option2";
  static const char kFalseOption2[] = "--nol2tpipsec-option2";
  static const char kProperty2[] = "L2TPIPSec.SomeProperty2";

  vector<string> options;
  EXPECT_FALSE(driver_->AppendFlag("L2TPIPSec.UnknownProperty",
                                   kTrueOption, kFalseOption, &options));
  EXPECT_TRUE(options.empty());

  SetArg(kProperty, "");
  EXPECT_FALSE(
      driver_->AppendFlag(kProperty, kTrueOption, kFalseOption, &options));
  EXPECT_TRUE(options.empty());

  SetArg(kProperty, "true");
  SetArg(kProperty2, "false");
  EXPECT_TRUE(
      driver_->AppendFlag(kProperty, kTrueOption, kFalseOption, &options));
  EXPECT_TRUE(
      driver_->AppendFlag(kProperty2, kTrueOption2, kFalseOption2, &options));
  EXPECT_EQ(2, options.size());
  EXPECT_EQ(kTrueOption, options[0]);
  EXPECT_EQ(kFalseOption2, options[1]);
}

TEST_F(L2TPIPSecDriverTest, GetLogin) {
  static const char kUser[] = "joesmith";
  static const char kPassword[] = "random-password";
  string user, password;
  SetArg(flimflam::kL2tpIpsecUserProperty, kUser);
  driver_->GetLogin(&user, &password);
  EXPECT_TRUE(user.empty());
  EXPECT_TRUE(password.empty());
  SetArg(flimflam::kL2tpIpsecUserProperty, "");
  SetArg(flimflam::kL2tpIpsecPasswordProperty, kPassword);
  driver_->GetLogin(&user, &password);
  EXPECT_TRUE(user.empty());
  EXPECT_TRUE(password.empty());
  SetArg(flimflam::kL2tpIpsecUserProperty, kUser);
  driver_->GetLogin(&user, &password);
  EXPECT_EQ(kUser, user);
  EXPECT_EQ(kPassword, password);
}

TEST_F(L2TPIPSecDriverTest, OnL2TPIPSecVPNDied) {
  const int kPID = 99999;
  driver_->child_watch_tag_ = 333;
  driver_->pid_ = kPID;
  EXPECT_CALL(process_killer_, Kill(_, _)).Times(0);
  L2TPIPSecDriver::OnL2TPIPSecVPNDied(kPID, 2, driver_);
  EXPECT_EQ(0, driver_->child_watch_tag_);
  EXPECT_EQ(0, driver_->pid_);
}

namespace {
MATCHER(CheckEnv, "") {
  if (!arg || !arg[0] || !arg[1] || !arg[2] || arg[3]) {
    return false;
  }
  return StartsWithASCII(arg[0], "CONNMAN_", true);
}
}  // namespace

TEST_F(L2TPIPSecDriverTest, SpawnL2TPIPSecVPN) {
  Error error;
  EXPECT_FALSE(driver_->SpawnL2TPIPSecVPN(&error));
  EXPECT_TRUE(error.IsFailure());

  static const char kHost[] = "192.168.2.254";
  SetArg(flimflam::kProviderHostProperty, kHost);
  driver_->rpc_task_.reset(new RPCTask(&control_, this));

  const int kPID = 234678;
  EXPECT_CALL(glib_,
              SpawnAsyncWithPipesCWD(_, CheckEnv(), _, _, _, _, _, _, _, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<5>(kPID), Return(true)));
  const int kTag = 6;
  EXPECT_CALL(glib_, ChildWatchAdd(kPID, &driver_->OnL2TPIPSecVPNDied, driver_))
      .WillOnce(Return(kTag));
  error.Reset();
  EXPECT_FALSE(driver_->SpawnL2TPIPSecVPN(&error));
  EXPECT_EQ(Error::kInternalError, error.type());
  error.Reset();
  EXPECT_TRUE(driver_->SpawnL2TPIPSecVPN(&error));
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kPID, driver_->pid_);
  EXPECT_EQ(kTag, driver_->child_watch_tag_);
}

TEST_F(L2TPIPSecDriverTest, Connect) {
  EXPECT_CALL(*service_, SetState(Service::kStateConfiguring));
  static const char kHost[] = "192.168.2.254";
  SetArg(flimflam::kProviderHostProperty, kHost);
  EXPECT_CALL(glib_, SpawnAsyncWithPipesCWD(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(glib_, ChildWatchAdd(_, _, _)).WillOnce(Return(1));
  Error error;
  driver_->Connect(service_, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_TRUE(driver_->IsConnectTimeoutStarted());
}

TEST_F(L2TPIPSecDriverTest, Disconnect) {
  driver_->device_ = device_;
  driver_->service_ = service_;
  EXPECT_CALL(*device_, OnDisconnected());
  EXPECT_CALL(*device_, SetEnabled(false));
  EXPECT_CALL(*service_, SetState(Service::kStateIdle));
  driver_->Disconnect();
  EXPECT_FALSE(driver_->device_);
  EXPECT_FALSE(driver_->service_);
}

TEST_F(L2TPIPSecDriverTest, OnConnectionDisconnected) {
  driver_->service_ = service_;
  EXPECT_CALL(*service_, SetState(Service::kStateFailure));
  driver_->OnConnectionDisconnected();
  EXPECT_FALSE(driver_->service_);
}

TEST_F(L2TPIPSecDriverTest, InitPropertyStore) {
  // Sanity test property store initialization.
  PropertyStore store;
  driver_->InitPropertyStore(&store);
  const string kUser = "joe";
  Error error;
  EXPECT_TRUE(
      store.SetStringProperty(flimflam::kL2tpIpsecUserProperty, kUser, &error));
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kUser,
            GetArgs()->LookupString(flimflam::kL2tpIpsecUserProperty, ""));
}

TEST_F(L2TPIPSecDriverTest, GetProvider) {
  PropertyStore store;
  driver_->InitPropertyStore(&store);
  PropertyStoreInspector inspector(&store);
  {
    KeyValueStore props;
    EXPECT_TRUE(
        inspector.GetKeyValueStoreProperty(
            flimflam::kProviderProperty, &props));
    EXPECT_TRUE(props.LookupBool(flimflam::kPassphraseRequiredProperty, false));
    EXPECT_TRUE(
        props.LookupBool(flimflam::kL2tpIpsecPskRequiredProperty, false));
  }
  {
    KeyValueStore props;
    SetArg(flimflam::kL2tpIpsecPasswordProperty, "random-password");
    SetArg(flimflam::kL2tpIpsecPskProperty, "random-psk");
    EXPECT_TRUE(
        inspector.GetKeyValueStoreProperty(
            flimflam::kProviderProperty, &props));
    EXPECT_FALSE(props.LookupBool(flimflam::kPassphraseRequiredProperty, true));
    EXPECT_FALSE(
        props.LookupBool(flimflam::kL2tpIpsecPskRequiredProperty, true));
  }
}

TEST_F(L2TPIPSecDriverTest, ParseIPConfiguration) {
  map<string, string> config;
  config["INTERNAL_IP4_ADDRESS"] = "4.5.6.7";
  config["EXTERNAL_IP4_ADDRESS"] = "33.44.55.66";
  config["GATEWAY_ADDRESS"] = "192.168.1.1";
  config["DNS1"] = "1.1.1.1";
  config["DNS2"] = "2.2.2.2";
  config["INTERNAL_IFNAME"] = "ppp0";
  config["LNS_ADDRESS"] = "99.88.77.66";
  config["foo"] = "bar";
  IPConfig::Properties props;
  string interface_name;
  L2TPIPSecDriver::ParseIPConfiguration(config, &props, &interface_name);
  EXPECT_EQ(IPAddress::kFamilyIPv4, props.address_family);
  EXPECT_EQ("4.5.6.7", props.address);
  EXPECT_EQ("33.44.55.66", props.peer_address);
  EXPECT_EQ("192.168.1.1", props.gateway);
  EXPECT_EQ("99.88.77.66", props.trusted_ip);
  ASSERT_EQ(2, props.dns_servers.size());
  EXPECT_EQ("1.1.1.1", props.dns_servers[0]);
  EXPECT_EQ("2.2.2.2", props.dns_servers[1]);
  EXPECT_EQ("ppp0", interface_name);
}

namespace {
MATCHER_P(IsIPAddress, address, "") {
  IPAddress ip_address(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(ip_address.SetAddressFromString(address));
  return ip_address.Equals(arg);
}
}  // namespace

TEST_F(L2TPIPSecDriverTest, Notify) {
  map<string, string> config;
  config["INTERNAL_IFNAME"] = kInterfaceName;
  EXPECT_CALL(device_info_, GetIndex(kInterfaceName))
      .WillOnce(Return(kInterfaceIndex));
  EXPECT_CALL(*device_, SetEnabled(true));
  EXPECT_CALL(*device_, UpdateIPConfig(_));
  driver_->device_ = device_;
  FilePath psk_file = SetupPSKFile();
  driver_->StartConnectTimeout();
  driver_->Notify("connect", config);
  EXPECT_FALSE(file_util::PathExists(psk_file));
  EXPECT_TRUE(driver_->psk_file_.empty());
  EXPECT_FALSE(driver_->IsConnectTimeoutStarted());
}

TEST_F(L2TPIPSecDriverTest, NotifyFail) {
  map<string, string> dict;
  driver_->device_ = device_;
  EXPECT_CALL(*device_, OnDisconnected());
  driver_->StartConnectTimeout();
  driver_->Notify("fail", dict);
  EXPECT_TRUE(driver_->IsConnectTimeoutStarted());
}

TEST_F(L2TPIPSecDriverTest, VerifyPaths) {
  // Ensure that the various path constants that the L2TP/IPSec driver uses
  // actually exists in the build image.  Due to build dependencies, they should
  // already exist by the time we run unit tests.

  // The L2TPIPSecDriver path constants are absolute.  FilePath::Append asserts
  // that its argument is not an absolute path, so we need to strip the leading
  // separators.  There's nothing built into FilePath to do so.
  static const char *kPaths[] = {
    L2TPIPSecDriver::kL2TPIPSecVPNPath,
    L2TPIPSecDriver::kPPPDPlugin,
  };
  for (size_t i = 0; i < arraysize(kPaths); i++) {
    string path(kPaths[i]);
    TrimString(path, FilePath::kSeparators, &path);
    EXPECT_TRUE(file_util::PathExists(FilePath(SYSROOT).Append(path)))
        << kPaths[i];
  }
}

}  // namespace shill
