// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/shill_client.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace arc_networkd {

class FakeShillClient : public ShillClient {
 public:
  explicit FakeShillClient(scoped_refptr<dbus::Bus> bus) : ShillClient(bus) {}

  bool GetDefaultInterface(std::string* ifname) override {
    *ifname = default_ifname_;
    return true;
  }

  void SetNewDefaultInterface(const std::string& ifname) {
    default_ifname_ = ifname;
  }

  void SetCurrentDefaultInterface(const std::string& ifname) {
    default_interface_ = ifname;
  }

  void NotifyManagerPropertyChange(const std::string& name,
                                   const brillo::Any& value) {
    OnManagerPropertyChange(name, value);
  }

 private:
  std::string default_ifname_;
};

class ShillClientTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), "org.chromium.flimflam", dbus::ObjectPath("/path"));
    // Set these expectations rather than just ignoring them to confirm
    // the ShillClient obtains the expected proxy and registers for
    // property changes.
    EXPECT_CALL(*mock_bus_, GetObjectProxy("org.chromium.flimflam", _))
        .WillRepeatedly(Return(mock_proxy_.get()));
    EXPECT_CALL(*mock_proxy_, ConnectToSignal("org.chromium.flimflam.Manager",
                                              "PropertyChanged", _, _))
        .Times(AnyNumber());

    client_ = std::make_unique<FakeShillClient>(mock_bus_);
    client_->RegisterDefaultInterfaceChangedHandler(
        base::Bind(&ShillClientTest::DefaultInterfaceChangedHandler,
                   base::Unretained(this)));
    client_->RegisterDevicesChangedHandler(base::Bind(
        &ShillClientTest::DevicesChangedHandler, base::Unretained(this)));
    default_ifname_.clear();
    devices_.clear();
  }

  void DefaultInterfaceChangedHandler(const std::string& name) {
    default_ifname_ = name;
  }

  void DevicesChangedHandler(const std::set<std::string>& devices) {
    devices_ = devices;
  }

 protected:
  std::string default_ifname_;
  std::set<std::string> devices_;
  std::unique_ptr<FakeShillClient> client_;

 private:
  scoped_refptr<dbus::MockBus> mock_bus_{
      new dbus::MockBus{dbus::Bus::Options{}}};
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
};

TEST_F(ShillClientTest, DevicesChangedHandlerCalledOnDevicesPropertyChange) {
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("eth0"),
                                           dbus::ObjectPath("wlan0")};
  auto value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  EXPECT_EQ(devices.size(), devices_.size());
  for (const auto d : devices) {
    EXPECT_NE(devices_.find(d.value()), devices_.end());
  }
}

TEST_F(ShillClientTest, VerifyDevicesPrefixStripped) {
  std::vector<dbus::ObjectPath> devices = {dbus::ObjectPath("/device/eth0")};
  auto value = brillo::Any(devices);
  client_->NotifyManagerPropertyChange(shill::kDevicesProperty, value);
  EXPECT_EQ(devices_.size(), 1);
  EXPECT_EQ(*devices_.begin(), "eth0");
}

TEST_F(ShillClientTest,
       DefaultInterfaceChangedHandlerCalledOnNewDefaultInterface) {
  client_->SetCurrentDefaultInterface("");
  client_->SetNewDefaultInterface("eth0");
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  EXPECT_EQ(default_ifname_, "eth0");
}

TEST_F(ShillClientTest, DefaultInterfaceChangedHandlerNotCalledForSameDefault) {
  client_->SetCurrentDefaultInterface("eth0");
  client_->SetNewDefaultInterface("eth0");
  client_->NotifyManagerPropertyChange(shill::kDefaultServiceProperty,
                                       brillo::Any() /* ignored */);
  // Implies the callback was not run.
  EXPECT_TRUE(default_ifname_.empty());
}

}  // namespace arc_networkd
