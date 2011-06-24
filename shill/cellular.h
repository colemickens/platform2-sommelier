// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_
#define SHILL_CELLULAR_

#include <string>

#include <base/basictypes.h>

#include "shill/cellular_service.h"
#include "shill/device.h"
#include "shill/shill_event.h"

namespace shill {

class Cellular : public Device {
 public:
  Cellular(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const std::string& link,
           int interface_index);
  ~Cellular();
  void Start();
  void Stop();
  bool TechnologyIs(Device::Technology type);

 private:
  ServiceRefPtr service_;
  bool service_registered_;

  // Properties
  bool allow_roaming_;
  std::string carrier_;
  std::string meid_;
  std::string imei_;
  std::string imsi_;
  std::string esn_;
  std::string mdn_;
  std::string min_;
  std::string model_id_;
  std::string manufacturer_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  int16 prl_version_;
  bool scanning_;
  uint16 scan_interval_;

  DISALLOW_COPY_AND_ASSIGN(Cellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_
