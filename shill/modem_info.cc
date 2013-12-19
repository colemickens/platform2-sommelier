// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_info.h"

#include <base/file_path.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular_operator_info.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/modem_manager.h"
#include "shill/pending_activation_store.h"

using base::FilePath;
using std::string;

// TODO(rochberg): Fix modemmanager-next-interfaces ebuild to include
// these so that we can simply include ModemManager.h and use these
// defines
#define MM_DBUS_PATH    "/org/freedesktop/ModemManager1"
#define MM_DBUS_SERVICE "org.freedesktop.ModemManager1"

namespace shill {

namespace {

const char kCromoService[] = "org.chromium.ModemManager";
const char kCromoPath[] = "/org/chromium/ModemManager";

const char kCellularOperatorInfoPath[] =
    "/usr/share/shill/cellular_operator_info";
const char kMobileProviderDBPath[] =
    "/usr/share/mobile-broadband-provider-info/serviceproviders.bfd";

}  // namespace

ModemInfo::ModemInfo(ControlInterface *control_interface,
                     EventDispatcher *dispatcher,
                     Metrics *metrics,
                     Manager *manager,
                     GLib *glib)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      glib_(glib),
      pending_activation_store_(NULL),
      provider_db_path_(kMobileProviderDBPath),
      provider_db_(NULL) {}

ModemInfo::~ModemInfo() {
  Stop();
}

void ModemInfo::Start() {
  pending_activation_store_.reset(new PendingActivationStore());
  pending_activation_store_->InitStorage(manager_->glib(),
      manager_->storage_path());
  cellular_operator_info_.reset(new CellularOperatorInfo());
  cellular_operator_info_->Load(FilePath(kCellularOperatorInfoPath));

  // TODO(petkov): Consider initializing the mobile provider database lazily
  // only if a GSM modem needs to be registered.
  provider_db_ = mobile_provider_open_db(provider_db_path_.c_str());
  PLOG_IF(WARNING, !provider_db_)
      << "Unable to load mobile provider database: ";
  RegisterModemManager(
      new ModemManagerClassic(kCromoService, kCromoPath, this));
  RegisterModemManager(new ModemManager1(MM_DBUS_SERVICE, MM_DBUS_PATH, this));
}

void ModemInfo::Stop() {
  pending_activation_store_.reset();
  cellular_operator_info_.reset();
  if(provider_db_)
    mobile_provider_close_db(provider_db_);
  provider_db_ = NULL;
  modem_managers_.clear();
}

void ModemInfo::OnDeviceInfoAvailable(const string &link_name) {
  for (ModemManagers::iterator it = modem_managers_.begin();
       it != modem_managers_.end(); ++it) {
    (*it)->OnDeviceInfoAvailable(link_name);
  }
}

void ModemInfo::set_pending_activation_store(
    PendingActivationStore *pending_activation_store) {
  pending_activation_store_.reset(pending_activation_store);
}

void ModemInfo::set_cellular_operator_info(
    CellularOperatorInfo *cellular_operator_info) {
  cellular_operator_info_.reset(cellular_operator_info);
}

void ModemInfo::RegisterModemManager(ModemManager *manager) {
  modem_managers_.push_back(manager);  // Passes ownership.
  manager->Start();
}

}  // namespace shill
