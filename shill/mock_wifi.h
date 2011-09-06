// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_WIFI_
#define SHILL_MOCK_WIFI_

#include <string>

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/key_value_store.h"
#include "shill/refptr_types.h"
#include "shill/wifi.h"
#include "shill/wifi_service.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;

class MockWiFi : public WiFi {
 public:
  MockWiFi(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const std::string &link_name,
           const std::string &address,
           int interface_index);
  virtual ~MockWiFi();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(Scan, void(Error *error));
  MOCK_METHOD2(GetService,
               WiFiServiceRefPtr(const KeyValueStore &args, Error *error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiFi);
};

}  // namespace shill

#endif  // SHILL_MOCK_WIFI_
