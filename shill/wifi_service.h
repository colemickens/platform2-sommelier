// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_SERVICE_
#define SHILL_WIFI_SERVICE_

#include <string>
#include <vector>

#include "shill/dbus_bindings/supplicant-interface.h"
#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Manager;

class WiFiService : public Service {
 public:
  WiFiService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Manager *manager,
              const WiFiRefPtr &device,
              const std::vector<uint8_t> ssid,
              uint32_t mode,
              const std::string &key_management,
              const std::string &name);
  ~WiFiService();
  void Connect();
  void Disconnect();
  uint32_t mode() const;
  const std::string &key_management() const;
  const std::vector<uint8_t> &ssid() const;

 protected:
  virtual std::string CalculateState() { return "idle"; }

 private:
  void ConnectTask();

  std::string GetDeviceRpcId();

  // Properties
  std::string passphrase_;
  bool need_passphrase_;
  std::string security_;
  uint8 strength_;
  const std::string type_;
  // TODO(cmasone): see if the below can be pulled from the endpoint associated
  // with this service instead.
  std::string auth_mode_;
  bool hidden_ssid_;
  uint16 frequency_;
  uint16 physical_mode_;
  uint16 hex_ssid_;

  ScopedRunnableMethodFactory<WiFiService> task_factory_;
  WiFiRefPtr wifi_;
  const std::vector<uint8_t> ssid_;
  const uint32_t mode_;
  DISALLOW_COPY_AND_ASSIGN(WiFiService);
};

}  // namespace shill

#endif  // SHILL_WIFI_SERVICE_
