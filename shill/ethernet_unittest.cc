// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet.h"

#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h
#include <linux/sockios.h>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/mock_device_info.h"
#include "shill/mock_dhcp_config.h"
#include "shill/mock_dhcp_provider.h"
#include "shill/mock_eap_credentials.h"
#include "shill/mock_eap_listener.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_ethernet_eap_provider.h"
#include "shill/mock_ethernet_service.h"
#include "shill/mock_glib.h"
#include "shill/mock_log.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_proxy_factory.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_service.h"
#include "shill/mock_sockets.h"
#include "shill/mock_supplicant_interface_proxy.h"
#include "shill/mock_supplicant_process_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/testing.h"
#include "shill/wpa_supplicant.h"

using std::string;
using testing::_;
using testing::AnyNumber;
using testing::EndsWith;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;
using testing::StrictMock;
using testing::Throw;

namespace shill {

class EthernetTest : public testing::Test {
 public:
  EthernetTest()
      : metrics_(nullptr),
        manager_(&control_interface_, NULL, &metrics_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &metrics_, &manager_),
        ethernet_(new Ethernet(&control_interface_,
                               &dispatcher_,
                               &metrics_,
                               &manager_,
                               kDeviceName,
                               kDeviceAddress,
                               kInterfaceIndex)),
        dhcp_config_(new MockDHCPConfig(&control_interface_,
                                        kDeviceName)),
        eap_listener_(new MockEapListener()),
        mock_sockets_(new StrictMock<MockSockets>()),
        mock_service_(new MockEthernetService(&control_interface_, &metrics_)),
        mock_eap_service_(new MockService(&control_interface_,
                                          &dispatcher_,
                                          &metrics_,
                                          &manager_)),
        supplicant_interface_proxy_(
            new NiceMock<MockSupplicantInterfaceProxy>()),
        supplicant_process_proxy_(new NiceMock<MockSupplicantProcessProxy>()) {}

  virtual void SetUp() {
    ethernet_->rtnl_handler_ = &rtnl_handler_;
    ethernet_->proxy_factory_ = &proxy_factory_;
    ethernet_->eap_listener_.reset(eap_listener_);  // Transfers ownership.
    ethernet_->sockets_.reset(mock_sockets_);  // Transfers ownership.

    ethernet_->set_dhcp_provider(&dhcp_provider_);
    ON_CALL(manager_, device_info()).WillByDefault(Return(&device_info_));
    EXPECT_CALL(manager_, UpdateEnabledTechnologies()).Times(AnyNumber());
    EXPECT_CALL(manager_, ethernet_eap_provider())
        .WillRepeatedly(Return(&ethernet_eap_provider_));
    ethernet_eap_provider_.set_service(mock_eap_service_);
  }

  virtual void TearDown() {
    ethernet_eap_provider_.set_service(NULL);
    ethernet_->set_dhcp_provider(NULL);
    ethernet_->eap_listener_.reset();
    ethernet_->sockets_.reset();
  }

 protected:
  static const char kDeviceName[];
  static const char kDeviceAddress[];
  static const char kInterfacePath[];
  static const int kInterfaceIndex;

  bool GetIsEapAuthenticated() { return ethernet_->is_eap_authenticated_; }
  void SetIsEapAuthenticated(bool is_eap_authenticated) {
    ethernet_->is_eap_authenticated_ = is_eap_authenticated;
  }
  bool GetIsEapDetected() { return ethernet_->is_eap_detected_; }
  void SetIsEapDetected(bool is_eap_detected) {
    ethernet_->is_eap_detected_ = is_eap_detected;
  }
  bool GetLinkUp() { return ethernet_->link_up_; }
  const ServiceRefPtr &GetSelectedService() {
    return ethernet_->selected_service();
  }
  ServiceRefPtr GetService() { return ethernet_->service_; }
  void SetService(const EthernetServiceRefPtr &service) {
    ethernet_->service_ = service;
  }
  const PropertyStore &GetStore() { return ethernet_->store(); }
  void StartEthernet() {
    EXPECT_CALL(rtnl_handler_,
                SetInterfaceFlags(kInterfaceIndex, IFF_UP, IFF_UP));
    ethernet_->Start(NULL, EnabledStateChangedCallback());
  }
  const SupplicantInterfaceProxyInterface *GetSupplicantInterfaceProxy() {
    return ethernet_->supplicant_interface_proxy_.get();
  }
  const SupplicantProcessProxyInterface *GetSupplicantProcessProxy() {
    return ethernet_->supplicant_process_proxy_.get();
  }
  const string &GetSupplicantInterfacePath() {
    return ethernet_->supplicant_interface_path_;
  }
  const string &GetSupplicantNetworkPath() {
    return ethernet_->supplicant_network_path_;
  }
  void SetSupplicantNetworkPath(const string &network_path) {
    ethernet_->supplicant_network_path_ = network_path;
  }
  bool InvokeStartSupplicant() {
    return ethernet_->StartSupplicant();
  }
  void InvokeStopSupplicant() {
    return ethernet_->StopSupplicant();
  }
  bool InvokeStartEapAuthentication() {
    return ethernet_->StartEapAuthentication();
  }
  void StartSupplicant() {
    MockSupplicantInterfaceProxy *interface_proxy =
        ExpectCreateSupplicantInterfaceProxy();
    MockSupplicantProcessProxy *process_proxy =
        ExpectCreateSupplicantProcessProxy();
    EXPECT_CALL(*process_proxy, CreateInterface(_))
        .WillOnce(Return(kInterfacePath));
    EXPECT_TRUE(InvokeStartSupplicant());
    EXPECT_EQ(interface_proxy, GetSupplicantInterfaceProxy());
    EXPECT_EQ(process_proxy, GetSupplicantProcessProxy());
    EXPECT_EQ(kInterfacePath, GetSupplicantInterfacePath());
  }
  void TriggerOnEapDetected() { ethernet_->OnEapDetected(); }
  void TriggerCertification(const string &subject, uint32 depth) {
    ethernet_->CertificationTask(subject, depth);
  }
  void TriggerTryEapAuthentication() {
    ethernet_->TryEapAuthenticationTask();
  }

  MockSupplicantInterfaceProxy *ExpectCreateSupplicantInterfaceProxy() {
    EXPECT_CALL(proxy_factory_,
                CreateSupplicantInterfaceProxy(_, DBus::Path(kInterfacePath),
                                               StrEq(WPASupplicant::kDBusAddr)))
        .WillOnce(ReturnAndReleasePointee(&supplicant_interface_proxy_));
    return supplicant_interface_proxy_.get();
  }
  MockSupplicantProcessProxy *ExpectCreateSupplicantProcessProxy() {
    EXPECT_CALL(proxy_factory_,
                CreateSupplicantProcessProxy(StrEq(WPASupplicant::kDBusPath),
                                             StrEq(WPASupplicant::kDBusAddr)))
        .WillOnce(ReturnAndReleasePointee(&supplicant_process_proxy_));
    return supplicant_process_proxy_.get();
  }

  StrictMock<MockEventDispatcher> dispatcher_;
  MockGLib glib_;
  NiceMockControl control_interface_;
  NiceMock<MockMetrics> metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  EthernetRefPtr ethernet_;
  MockEthernetEapProvider ethernet_eap_provider_;
  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;

  // Owned by Ethernet instance, but tracked here for expectations.
  MockEapListener *eap_listener_;
  MockSockets *mock_sockets_;

  MockRTNLHandler rtnl_handler_;
  scoped_refptr<MockEthernetService> mock_service_;
  scoped_refptr<MockService> mock_eap_service_;
  NiceMock<MockProxyFactory> proxy_factory_;
  scoped_ptr<MockSupplicantInterfaceProxy> supplicant_interface_proxy_;
  scoped_ptr<MockSupplicantProcessProxy> supplicant_process_proxy_;
};

// static
const char EthernetTest::kDeviceName[] = "eth0";
const char EthernetTest::kDeviceAddress[] = "000102030405";
const char EthernetTest::kInterfacePath[] = "/interface/path";
const int EthernetTest::kInterfaceIndex = 123;

TEST_F(EthernetTest, Construct) {
  EXPECT_FALSE(GetLinkUp());
  EXPECT_FALSE(GetIsEapAuthenticated());
  EXPECT_FALSE(GetIsEapDetected());
  EXPECT_TRUE(GetStore().Contains(kEapAuthenticationCompletedProperty));
  EXPECT_TRUE(GetStore().Contains(kEapAuthenticatorDetectedProperty));
  EXPECT_EQ(NULL, GetService().get());
}

TEST_F(EthernetTest, StartStop) {
  StartEthernet();

  EXPECT_FALSE(GetService().get() == NULL);

  Service* service = GetService().get();
  EXPECT_CALL(manager_, DeregisterService(Eq(service)));
  ethernet_->Stop(NULL, EnabledStateChangedCallback());
  EXPECT_EQ(NULL, GetService().get());
}

TEST_F(EthernetTest, LinkEvent) {
  StartEthernet();

  // Link-down event while already down.
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);
  EXPECT_CALL(*eap_listener_, Start()).Times(0);
  ethernet_->LinkEvent(0, IFF_LOWER_UP);
  EXPECT_FALSE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());
  Mock::VerifyAndClearExpectations(&manager_);

  // Link-up event while down.
  int kFakeFd = 789;
  EXPECT_CALL(manager_, RegisterService(GetService()));
  EXPECT_CALL(*eap_listener_, Start());
  EXPECT_CALL(*mock_sockets_, Socket(_, _, _)).WillOnce(Return(kFakeFd));
  EXPECT_CALL(*mock_sockets_, Ioctl(kFakeFd, SIOCETHTOOL, _));
  EXPECT_CALL(*mock_sockets_, Close(kFakeFd));
  ethernet_->LinkEvent(IFF_LOWER_UP, 0);
  EXPECT_TRUE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());
  Mock::VerifyAndClearExpectations(&manager_);

  // Link-up event while already up.
  EXPECT_CALL(manager_, RegisterService(GetService())).Times(0);
  EXPECT_CALL(*eap_listener_, Start()).Times(0);
  ethernet_->LinkEvent(IFF_LOWER_UP, 0);
  EXPECT_TRUE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());
  Mock::VerifyAndClearExpectations(&manager_);

  // Link-down event while up.
  SetIsEapDetected(true);
  // This is done in SetUp, but we have to reestablish this after calling
  // VerifyAndClearExpectations() above.
  EXPECT_CALL(manager_, ethernet_eap_provider())
      .WillRepeatedly(Return(&ethernet_eap_provider_));
  EXPECT_CALL(ethernet_eap_provider_,
              ClearCredentialChangeCallback(ethernet_.get()));
  EXPECT_CALL(manager_, DeregisterService(GetService()));
  EXPECT_CALL(*eap_listener_, Stop());
  ethernet_->LinkEvent(0, IFF_LOWER_UP);
  EXPECT_FALSE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());

  // Restore this expectation during shutdown.
  EXPECT_CALL(manager_, UpdateEnabledTechnologies()).Times(AnyNumber());
}

TEST_F(EthernetTest, ConnectToFailure) {
  StartEthernet();
  SetService(mock_service_);
  EXPECT_EQ(NULL, GetSelectedService().get());
  EXPECT_CALL(dhcp_provider_, CreateConfig(_, _, _, _, _)).
      WillOnce(Return(dhcp_config_));
  EXPECT_CALL(*dhcp_config_.get(), RequestIP()).WillOnce(Return(false));
  EXPECT_CALL(dispatcher_, PostTask(_));  // Posts ConfigureStaticIPTask.
  EXPECT_CALL(*mock_service_, SetState(Service::kStateFailure));
  ethernet_->ConnectTo(mock_service_);
  EXPECT_EQ(mock_service_, GetSelectedService().get());
}

TEST_F(EthernetTest, ConnectToSuccess) {
  StartEthernet();
  SetService(mock_service_);
  EXPECT_EQ(NULL, GetSelectedService().get());
  EXPECT_CALL(dhcp_provider_, CreateConfig(_, _, _, _, _)).
      WillOnce(Return(dhcp_config_));
  EXPECT_CALL(*dhcp_config_.get(), RequestIP()).WillOnce(Return(true));
  EXPECT_CALL(dispatcher_, PostTask(_));  // Posts ConfigureStaticIPTask.
  EXPECT_CALL(*mock_service_, SetState(Service::kStateConfiguring));
  ethernet_->ConnectTo(mock_service_.get());
  EXPECT_EQ(GetService().get(), GetSelectedService().get());
  Mock::VerifyAndClearExpectations(mock_service_);

  EXPECT_CALL(*mock_service_, SetState(Service::kStateIdle));
  ethernet_->DisconnectFrom(mock_service_);
  EXPECT_EQ(NULL, GetSelectedService().get());
}

TEST_F(EthernetTest, OnEapDetected) {
  EXPECT_FALSE(GetIsEapDetected());
  EXPECT_CALL(*eap_listener_, Stop());
  EXPECT_CALL(ethernet_eap_provider_,
              SetCredentialChangeCallback(ethernet_.get(), _));
  EXPECT_CALL(dispatcher_, PostTask(_));  // Posts TryEapAuthenticationTask.
  TriggerOnEapDetected();
  EXPECT_TRUE(GetIsEapDetected());
}

TEST_F(EthernetTest, TryEapAuthenticationNoService) {
  EXPECT_CALL(*mock_eap_service_, Is8021xConnectable()).Times(0);
  NiceScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("Service is missing; "
                                "not doing EAP authentication.")));
  TriggerTryEapAuthentication();
}

TEST_F(EthernetTest, TryEapAuthenticationNotConnectableNotAuthenticated) {
  SetService(mock_service_);
  EXPECT_CALL(*mock_eap_service_, Is8021xConnectable()).WillOnce(Return(false));
  NiceScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("EAP Service lacks 802.1X credentials; "
                                "not doing EAP authentication.")));
  TriggerTryEapAuthentication();
  SetService(NULL);
}

TEST_F(EthernetTest, TryEapAuthenticationNotConnectableAuthenticated) {
  SetService(mock_service_);
  SetIsEapAuthenticated(true);
  EXPECT_CALL(*mock_eap_service_, Is8021xConnectable()).WillOnce(Return(false));
  NiceScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(log, Log(logging::LOG_INFO, _,
                       EndsWith("EAP Service lost 802.1X credentials; "
                                "terminating EAP authentication.")));
  TriggerTryEapAuthentication();
  EXPECT_FALSE(GetIsEapAuthenticated());
}

TEST_F(EthernetTest, TryEapAuthenticationEapNotDetected) {
  SetService(mock_service_);
  EXPECT_CALL(*mock_eap_service_, Is8021xConnectable()).WillOnce(Return(true));
  NiceScopedMockLog log;
  EXPECT_CALL(log, Log(logging::LOG_WARNING, _,
                       EndsWith("EAP authenticator not detected; "
                                "not doing EAP authentication.")));
  TriggerTryEapAuthentication();
}

TEST_F(EthernetTest, StartSupplicant) {
  // Save the mock proxy pointers before the Ethernet instance accepts it.
  MockSupplicantInterfaceProxy *interface_proxy =
      supplicant_interface_proxy_.get();
  MockSupplicantProcessProxy *process_proxy = supplicant_process_proxy_.get();

  StartSupplicant();

  // Starting it again should not invoke another call to create an interface.
  Mock::VerifyAndClearExpectations(process_proxy);
  EXPECT_CALL(*process_proxy, CreateInterface(_)).Times(0);
  EXPECT_TRUE(InvokeStartSupplicant());

  // Also, the mock pointers should remain; if the MockProxyFactory was
  // invoked again, they would be NULL.
  EXPECT_EQ(interface_proxy, GetSupplicantInterfaceProxy());
  EXPECT_EQ(process_proxy, GetSupplicantProcessProxy());
  EXPECT_EQ(kInterfacePath, GetSupplicantInterfacePath());
}

TEST_F(EthernetTest, StartSupplicantWithInterfaceExistsException) {
  MockSupplicantProcessProxy *process_proxy =
      ExpectCreateSupplicantProcessProxy();
  MockSupplicantInterfaceProxy *interface_proxy =
      ExpectCreateSupplicantInterfaceProxy();
  EXPECT_CALL(*process_proxy, CreateInterface(_))
      .WillOnce(Throw(DBus::Error(
          "fi.w1.wpa_supplicant1.InterfaceExists",
          "test threw fi.w1.wpa_supplicant1.InterfaceExists")));
  EXPECT_CALL(*process_proxy, GetInterface(kDeviceName))
      .WillOnce(Return(kInterfacePath));
  EXPECT_TRUE(InvokeStartSupplicant());
  EXPECT_EQ(interface_proxy, GetSupplicantInterfaceProxy());
  EXPECT_EQ(process_proxy, GetSupplicantProcessProxy());
  EXPECT_EQ(kInterfacePath, GetSupplicantInterfacePath());
}

TEST_F(EthernetTest, StartSupplicantWithUnknownException) {
  MockSupplicantProcessProxy *process_proxy =
      ExpectCreateSupplicantProcessProxy();
  EXPECT_CALL(*process_proxy, CreateInterface(_))
      .WillOnce(Throw(DBus::Error(
          "fi.w1.wpa_supplicant1.UnknownError",
          "test threw fi.w1.wpa_supplicant1.UnknownError")));
  EXPECT_CALL(*process_proxy, GetInterface(kDeviceName)).Times(0);
  EXPECT_FALSE(InvokeStartSupplicant());
  EXPECT_EQ(NULL, GetSupplicantInterfaceProxy());
  EXPECT_EQ(NULL, GetSupplicantProcessProxy());
  EXPECT_EQ("", GetSupplicantInterfacePath());
}

TEST_F(EthernetTest, StartEapAuthentication) {
  MockSupplicantInterfaceProxy *interface_proxy =
      supplicant_interface_proxy_.get();

  StartSupplicant();
  SetService(mock_service_);

  EXPECT_CALL(*mock_service_, ClearEAPCertification());
  MockEapCredentials mock_eap_credentials;
  EXPECT_CALL(*mock_eap_service_, eap())
      .WillOnce(Return(&mock_eap_credentials));
  EXPECT_CALL(mock_eap_credentials, PopulateSupplicantProperties(_, _));
  EXPECT_CALL(*interface_proxy, RemoveNetwork(_)).Times(0);
  EXPECT_CALL(*interface_proxy, AddNetwork(_))
      .WillOnce(Throw(DBus::Error(
          "fi.w1.wpa_supplicant1.UnknownError",
          "test threw fi.w1.wpa_supplicant1.UnknownError")));
  EXPECT_CALL(*interface_proxy, SelectNetwork(_)).Times(0);
  EXPECT_CALL(*interface_proxy, EAPLogon()).Times(0);
  EXPECT_FALSE(InvokeStartEapAuthentication());
  Mock::VerifyAndClearExpectations(mock_service_);
  Mock::VerifyAndClearExpectations(mock_eap_service_);
  Mock::VerifyAndClearExpectations(interface_proxy);
  EXPECT_EQ("", GetSupplicantNetworkPath());

  EXPECT_CALL(*mock_service_, ClearEAPCertification());
  EXPECT_CALL(*interface_proxy, RemoveNetwork(_)).Times(0);
  EXPECT_CALL(*mock_eap_service_, eap())
      .WillOnce(Return(&mock_eap_credentials));
  EXPECT_CALL(mock_eap_credentials, PopulateSupplicantProperties(_, _));
  const char kFirstNetworkPath[] = "/network/first-path";
  EXPECT_CALL(*interface_proxy, AddNetwork(_))
      .WillOnce(Return(kFirstNetworkPath));
  EXPECT_CALL(*interface_proxy, SelectNetwork(StrEq(kFirstNetworkPath)));
  EXPECT_CALL(*interface_proxy, EAPLogon());
  EXPECT_TRUE(InvokeStartEapAuthentication());
  Mock::VerifyAndClearExpectations(mock_service_);
  Mock::VerifyAndClearExpectations(mock_eap_service_);
  Mock::VerifyAndClearExpectations(&mock_eap_credentials);
  Mock::VerifyAndClearExpectations(interface_proxy);
  EXPECT_EQ(kFirstNetworkPath, GetSupplicantNetworkPath());

  EXPECT_CALL(*mock_service_, ClearEAPCertification());
  EXPECT_CALL(*interface_proxy, RemoveNetwork(StrEq(kFirstNetworkPath)));
  EXPECT_CALL(*mock_eap_service_, eap())
      .WillOnce(Return(&mock_eap_credentials));
  EXPECT_CALL(mock_eap_credentials, PopulateSupplicantProperties(_, _));
  const char kSecondNetworkPath[] = "/network/second-path";
  EXPECT_CALL(*interface_proxy, AddNetwork(_))
      .WillOnce(Return(kSecondNetworkPath));
  EXPECT_CALL(*interface_proxy, SelectNetwork(StrEq(kSecondNetworkPath)));
  EXPECT_CALL(*interface_proxy, EAPLogon());
  EXPECT_TRUE(InvokeStartEapAuthentication());
  EXPECT_EQ(kSecondNetworkPath, GetSupplicantNetworkPath());
}

TEST_F(EthernetTest, StopSupplicant) {
  MockSupplicantProcessProxy *process_proxy = supplicant_process_proxy_.get();
  MockSupplicantInterfaceProxy *interface_proxy =
      supplicant_interface_proxy_.get();
  StartSupplicant();
  SetIsEapAuthenticated(true);
  SetSupplicantNetworkPath("/network/1");
  EXPECT_CALL(*interface_proxy, EAPLogoff());
  EXPECT_CALL(*process_proxy, RemoveInterface(StrEq(kInterfacePath)));
  InvokeStopSupplicant();
  EXPECT_EQ(NULL, GetSupplicantInterfaceProxy());
  EXPECT_EQ(NULL, GetSupplicantProcessProxy());
  EXPECT_EQ("", GetSupplicantInterfacePath());
  EXPECT_EQ("", GetSupplicantNetworkPath());
  EXPECT_FALSE(GetIsEapAuthenticated());
}

TEST_F(EthernetTest, Certification) {
  const string kSubjectName("subject-name");
  const uint32 kDepth = 123;
  // Should not crash due to no service_.
  TriggerCertification(kSubjectName, kDepth);
  EXPECT_CALL(*mock_service_, AddEAPCertification(kSubjectName, kDepth));
  SetService(mock_service_);
  TriggerCertification(kSubjectName, kDepth);
}

}  // namespace shill
