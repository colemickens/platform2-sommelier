// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_WIFI_SERVICE_
#define SHILL_MOCK_WIFI_SERVICE_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/wifi_service.h"

namespace shill {

class MockWiFiService : public WiFiService {
 public:
  MockWiFiService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Metrics *metrics,
                  Manager *manager,
                  WiFiProvider *provider,
                  const std::vector<uint8_t> &ssid,
                  const std::string &mode,
                  const std::string &security,
                  bool hidden_ssid);
  virtual ~MockWiFiService();

  MOCK_METHOD2(Configure, void(const KeyValueStore &args, Error *error));
  MOCK_METHOD1(SetFailure, void(ConnectFailure failure));
  MOCK_METHOD1(SetFailureSilent, void(ConnectFailure failure));
  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_METHOD2(AddEAPCertification, bool(const std::string &name,
                                         size_t depth));
  MOCK_METHOD0(HasRecentConnectionIssues, bool());
  MOCK_METHOD0(AddSuspectedCredentialFailure, bool());
  MOCK_METHOD0(ResetSuspectedCredentialFailures, void());
  MOCK_METHOD1(AddEndpoint,
               void(const WiFiEndpointConstRefPtr &endpoint));
  MOCK_METHOD1(RemoveEndpoint,
               void(const WiFiEndpointConstRefPtr &endpoint));
  MOCK_METHOD1(NotifyCurrentEndpoint,
               void(const WiFiEndpointConstRefPtr &endpoint));
  MOCK_METHOD1(NotifyEndpointUpdated,
               void(const WiFiEndpointConstRefPtr &endpoint));
  MOCK_METHOD2(DisconnectWithFailure,
               void(ConnectFailure failure, Error *error));
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
  MOCK_CONST_METHOD0(GetEndpointCount, int());
  MOCK_CONST_METHOD0(HasEndpoints, bool());
  MOCK_CONST_METHOD0(IsRemembered, bool());
  MOCK_METHOD0(ResetWiFi, void());
  MOCK_CONST_METHOD0(GetSupplicantConfigurationParameters, DBusPropertiesMap());
  MOCK_CONST_METHOD1(IsAutoConnectable, bool(const char **reason));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiFiService);
};

}  // namespace shill

#endif  // SHILL_MOCK_WIFI_SERVICE_
