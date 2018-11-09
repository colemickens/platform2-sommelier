// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DHCP_SERVER_H_
#define APMANAGER_DHCP_SERVER_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <shill/net/ip_address.h>
#include <shill/net/rtnl_handler.h>

#include "apmanager/file_writer.h"
#include "apmanager/process_factory.h"

namespace apmanager {

class DHCPServer {
 public:
  DHCPServer(uint16_t server_address_index,
             const std::string& interface_name);
  virtual ~DHCPServer();

  // Start the DHCP server
  virtual bool Start();

 private:
  friend class DHCPServerTest;

  std::string GenerateConfigFile();

  static const char kDnsmasqPath[];
  static const char kDnsmasqConfigFilePathFormat[];
  static const char kDHCPLeasesFilePathFormat[];
  static const char kServerAddressFormat[];
  static const char kAddressRangeLowFormat[];
  static const char kAddressRangeHighFormat[];
  static const int kServerAddressPrefix;
  static const int kTerminationTimeoutSeconds;

  uint16_t server_address_index_;
  std::string interface_name_;
  shill::IPAddress server_address_;
  std::unique_ptr<brillo::Process> dnsmasq_process_;
  shill::RTNLHandler* rtnl_handler_;
  FileWriter* file_writer_;
  ProcessFactory* process_factory_;

  DISALLOW_COPY_AND_ASSIGN(DHCPServer);
};

}  // namespace apmanager

#endif  // APMANAGER_DHCP_SERVER_H_
