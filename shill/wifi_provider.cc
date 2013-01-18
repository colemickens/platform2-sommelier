// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_provider.h"

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"

namespace shill {

WiFiProvider::WiFiProvider(ControlInterface *control_interface,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager) {}

WiFiProvider::~WiFiProvider() {}

void WiFiProvider::Start() {
  NOTIMPLEMENTED();
}

void WiFiProvider::Stop() {
  NOTIMPLEMENTED();
}

void WiFiProvider::CreateServicesFromProfile(ProfileRefPtr profile) {
  NOTIMPLEMENTED();
}

WiFiServiceRefPtr WiFiProvider::GetService(
    const KeyValueStore &args, Error *error) {
  NOTIMPLEMENTED();
  return NULL;
}

WiFiServiceRefPtr WiFiProvider::FindServiceForEndpoint(
    const WiFiEndpoint &endpoint) {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace shill
