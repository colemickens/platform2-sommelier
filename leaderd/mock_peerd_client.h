// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LEADERD_MOCK_PEERD_CLIENT_H_
#define LEADERD_MOCK_PEERD_CLIENT_H_

#include <map>
#include <string>
#include <vector>
#include <gmock/gmock.h>
#include "leaderd/peerd_client.h"

namespace leaderd {

class MockManagerInterface
    : public org::chromium::peerd::ManagerProxyInterface {
 public:
  MockManagerInterface() = default;
  ~MockManagerInterface() = default;

  MOCK_METHOD5(StartMonitoring, bool(const std::vector<std::string>&,
                                     const chromeos::VariantDictionary&,
                                     std::string*, chromeos::ErrorPtr*, int));

  MOCK_METHOD5(StartMonitoringAsync,
               void(const std::vector<std::string>&,
                    const chromeos::VariantDictionary&,
                    const base::Callback<void(const std::string&)>&,
                    const base::Callback<void(chromeos::Error*)>&, int));

  MOCK_METHOD3(StopMonitoring,
               bool(const std::string&, chromeos::ErrorPtr*, int));

  MOCK_METHOD4(StopMonitoringAsync,
               void(const std::string&, const base::Callback<void()>&,
                    const base::Callback<void(chromeos::Error*)>&, int));

  MOCK_METHOD5(ExposeService, bool(const std::string&,
                                   const std::map<std::string, std::string>&,
                                   const chromeos::VariantDictionary&,
                                   chromeos::ErrorPtr*, int));

  MOCK_METHOD6(ExposeServiceAsync,
               void(const std::string&,
                    const std::map<std::string, std::string>&,
                    const chromeos::VariantDictionary&,
                    const base::Callback<void()>&,
                    const base::Callback<void(chromeos::Error*)>&, int));

  MOCK_METHOD3(RemoveExposedService,
               bool(const std::string&, chromeos::ErrorPtr*, int));

  MOCK_METHOD4(RemoveExposedServiceAsync,
               void(const std::string&, const base::Callback<void()>&,
                    const base::Callback<void(chromeos::Error*)>&, int));

  MOCK_METHOD3(Ping, bool(std::string*, chromeos::ErrorPtr*, int));

  MOCK_METHOD3(PingAsync,
               void(const base::Callback<void(const std::string&)>&,
                    const base::Callback<void(chromeos::Error*)>&, int));

  MOCK_CONST_METHOD0(monitored_technologies, const std::vector<std::string>&());
};

class MockPeerdClient : public PeerdClient {
 public:
  MockPeerdClient() = default;
  ~MockPeerdClient() override = default;

  MOCK_METHOD0(GetManagerProxy, org::chromium::peerd::ManagerProxyInterface*());
  MOCK_METHOD1(GetPeerProxy, org::chromium::peerd::PeerProxyInterface*(
                                 const dbus::ObjectPath& object_path));
  MOCK_METHOD1(GetServiceProxy, org::chromium::peerd::ServiceProxyInterface*(
                                    const dbus::ObjectPath& object_path));
};

}  // namespace leaderd

#endif  // LEADERD_MOCK_PEERD_CLIENT_H_
