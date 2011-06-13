// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_SERVICE_
#define SHILL_CELLULAR_SERVICE_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/cellular.h"
#include "shill/device.h"
#include "shill/shill_event.h"
#include "shill/service.h"

namespace shill {

class Cellular;

class CellularService : public Service {
 public:
  CellularService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Cellular *device,
                  const std::string& name);
  ~CellularService();
  void Connect();
  void Disconnect();

  // Implementation of PropertyStoreInterface
  bool Contains(const std::string &property);

 protected:
  virtual std::string CalculateState() { return "idle"; }

  // Properties
  std::string activation_state_;
  std::string operator_name_;
  std::string operator_code_;
  std::string network_tech_;
  std::string roaming_state_;
  std::string payment_url_;
  uint8 strength_;

  std::map<std::string, std::string> apn_info_;
  std::map<std::string, std::string> last_good_apn_info_;

 private:
  Cellular *cellular_;
  const std::string type_;
  DISALLOW_COPY_AND_ASSIGN(CellularService);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_SERVICE_
