// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem_info.h"

#include <base/files/file_path.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular/modem_manager.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/pending_activation_store.h"

using base::FilePath;
using std::string;

namespace shill {

ModemInfo::ModemInfo(ControlInterface* control_interface,
                     EventDispatcher* dispatcher,
                     Metrics* metrics,
                     Manager* manager,
                     GLib* glib)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      glib_(glib) {}

ModemInfo::~ModemInfo() {
  Stop();
}

void ModemInfo::Start() {
  pending_activation_store_.reset(new PendingActivationStore());
  pending_activation_store_->InitStorage(manager_->storage_path());

  RegisterModemManager(new ModemManagerClassic(control_interface_,
                                               cromo::kCromoServiceName,
                                               cromo::kCromoServicePath,
                                               this));
  RegisterModemManager(
      new ModemManager1(control_interface_,
                        modemmanager::kModemManager1ServiceName,
                        modemmanager::kModemManager1ServicePath,
                        this));
}

void ModemInfo::Stop() {
  pending_activation_store_.reset();
  modem_managers_.clear();
}

void ModemInfo::OnDeviceInfoAvailable(const string& link_name) {
  for (const auto& manager : modem_managers_) {
    manager->OnDeviceInfoAvailable(link_name);
  }
}

void ModemInfo::set_pending_activation_store(
    PendingActivationStore* pending_activation_store) {
  pending_activation_store_.reset(pending_activation_store);
}

void ModemInfo::RegisterModemManager(ModemManager* manager) {
  modem_managers_.push_back(manager);  // Passes ownership.
  manager->Start();
}

}  // namespace shill
