//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef PROXY_RPC_DATA_TYPES_H
#define PROXY_RPC_DATA_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <string>

#include "proxy_rpc_security_types.h"

// TODO: Only creating the datatypes. Need to figure out how to handle it.
class ProxyRpcDataType {
 public:
   ProxyRpcDataType() {}
};

// Describes parameters used in WiFi connection attempts."""
class AssociationParameters : public xmlrpc_types.XmlRpcStruct {
 public:
  const int kDefaultDiscoveryTimeout = 15;
  const int kDefaultAssociationTimeout = 15;
  const int kDefaultConfigurationTimeout = 15;
  const char kStationTypeManaged[] = "managed";
  const char kStationTypeIbss[] = "ibss";

 protected:
  std::string ssid_;
  SecurityConfig security_config_;
  int discovery_timeout_;
  int association_timeout_;
  int configuration_timeout_;
  bool is_hidden_;
  bool save_credentials_;
  std::string station_type_;
  bool expect_failure_;
  std::string service_guid_;
  bool expect_failure_;
  bool auto_connect_;
  BgscanConfiguration bgscan_config_;
};

// Describes the result of an association attempt.
class AssociationResult : public ProxyRpcDataType {
 public:

 protected:
  bool success_;
  int discovery_time_;
  int association_timeout_;
  int configuration_timeout_;
  int failure_reason_;
};

// Describes how to configure wpa_supplicant on a DUT.
class BgscanConfiguration : public ProxyRpcDataType {
 public:
  const int kDefaultShortIntervalSeconds = 30;
  const int kDefaultLongIntervalSeconds = 180;
  const int kDefaultignalThreshold = -50;
  const char kScanMethodDefault[] = "default";
  const char kScanMethodNone[] = "none";
  const char kScanMethodSimple[] = "simple";

 protected:
  std::string interface_;
  int signal_;
  int short_interval_;
  int long_interval_;
  std::string scan_method_;
};

// Describes a group of optional settings for use with ConfigureService.
// The Manager in shill has a method ConfigureService which takes a dictionary
// of parameters, and uses some of them to look up a service, and sets the
// remainder of the properties on the service.  This struct represents
// some of the optional parameters that can be set in this way.  Current
// consumers of this interface look up the service by GUID.
class ConfigureServiceParameters : public ProxyRpcDataType {
 public:

 protected:
  std::string service_guid_;
  std::string passphrase_;
  bool auto_connect_;
};

#endif // PROXY_RPC_DATA_TYPES_H
