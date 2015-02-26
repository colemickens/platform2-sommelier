// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyche.h"

#include <stdio.h>

#include <base/logging.h>

namespace psyche {

Psyche::Psyche() {}

Psyche::~Psyche() {}

void Psyche::Init() {
  service_info_map_.clear();
}

bool Psyche::AddService(const std::string& service_name,
                        bool is_running,
                        bool is_ephemeral) {
  if (service_info_map_.find(service_name) != service_info_map_.end()) {
    LOG(ERROR) << "Error trying to add a service that already exists.";
    return false;
  }
  ServiceInfo service_info;
  service_info.is_running = is_running;
  service_info.is_ephemeral = is_ephemeral;
  service_info_map_[service_name] = service_info;
  return true;
}

bool Psyche::FindServiceStatus(const std::string& service_name,
                               ServiceInfo* status) {
  if (service_info_map_.find(service_name) == service_info_map_.end()) {
    LOG(ERROR) << "Service " << service_name << " does not exist.";
    return false;
  }
  *status = service_info_map_[service_name];
  return true;
}

}  // namespace psyche
