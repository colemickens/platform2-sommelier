// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_PROVIDER_
#define SHILL_WIFI_PROVIDER_

#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class KeyValueStore;
class Manager;
class Metrics;
class WiFiEndpoint;
class WiFiService;

// The WiFi Provider is the holder of all WiFi Services.  It holds both
// visible (created due to an Endpoint becoming visible) and invisible
// (created due to user or storage configuration) Services.
class WiFiProvider {
 public:
  WiFiProvider(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager);
  virtual ~WiFiProvider();

  virtual void Start();
  virtual void Stop();

  // Called by Manager.
  void CreateServicesFromProfile(ProfileRefPtr profile);
  virtual WiFiServiceRefPtr GetService(const KeyValueStore &args, Error *error);

  // Called by WiFi device.
  WiFiServiceRefPtr FindServiceForEndpoint(const WiFiEndpoint &endpoint);

 private:
  friend class WiFiProviderTest;

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;

  DISALLOW_COPY_AND_ASSIGN(WiFiProvider);
};

}  // namespace shill

#endif  // SHILL_WIFI_PROVIDER_
