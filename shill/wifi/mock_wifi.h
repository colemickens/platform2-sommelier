// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_MOCK_WIFI_H_
#define SHILL_WIFI_MOCK_WIFI_H_

#include <string>

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/key_value_store.h"
#include "shill/refptr_types.h"
#include "shill/wifi/wake_on_wifi.h"
#include "shill/wifi/wifi.h"
#include "shill/wifi/wifi_endpoint.h"
#include "shill/wifi/wifi_service.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;

class MockWiFi : public WiFi {
 public:
  // MockWiFi takes ownership of the wake_on_wifi pointer passed to it.
  // This is not exposed in the constructor type because gmock doesn't
  // provide the ability to forward arguments that aren't const &...
  MockWiFi(Manager* manager,
           const std::string& link_name,
           const std::string& address,
           int interface_index,
           WakeOnWiFiInterface* wake_on_wifi);
  ~MockWiFi() override;

  MOCK_METHOD2(Start, void(Error* error,
                           const EnabledStateChangedCallback& callback));
  MOCK_METHOD2(Stop, void(Error* error,
                          const EnabledStateChangedCallback& callback));
  MOCK_METHOD2(Scan, void(Error* error, const std::string& reason));
  MOCK_METHOD1(DisconnectFromIfActive, void(WiFiService* service));
  MOCK_METHOD1(DisconnectFrom, void(WiFiService* service));
  MOCK_METHOD1(ClearCachedCredentials, void(const WiFiService* service));
  MOCK_METHOD1(ConnectTo, void(WiFiService* service));
  MOCK_CONST_METHOD0(IsIdle, bool());
  MOCK_METHOD1(NotifyEndpointChanged,
               void(const WiFiEndpointConstRefPtr& endpoint));
  MOCK_METHOD1(DestroyIPConfigLease, void(const std::string&));
  MOCK_CONST_METHOD0(IsConnectedViaTether, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiFi);
};

}  // namespace shill

#endif  // SHILL_WIFI_MOCK_WIFI_H_
