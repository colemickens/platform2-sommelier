// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/cromo_server.h"

#include <dbus/dbus.h>
#include <mm/mm-modem.h>

#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/glib-integration.h>

#include "cromo/carrier.h"
#include "cromo/modem_handler.h"
#include "cromo/plugin_manager.h"
#include "cromo/syslog_helper.h"

using std::vector;

const char CromoServer::kServiceName[] = "org.chromium.ModemManager";
const char CromoServer::kServicePath[] = "/org/chromium/ModemManager";

static const char kDBusInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";

CromoServer::CromoServer(
    DBus::Connection& connection)  // NOLINT(runtime/references)
    : DBus::ObjectAdaptor(connection, kServicePath),
      metrics_lib_(new MetricsLibrary()) {}

CromoServer::~CromoServer() {}

vector<DBus::Path> CromoServer::EnumerateDevices(
    DBus::Error& error) {  // NOLINT(runtime/references)
  vector<DBus::Path> allpaths;

  for (auto& modem_handler : modem_handlers_) {
    vector<DBus::Path> paths = modem_handler->EnumerateDevices(error);
    allpaths.insert(allpaths.end(), paths.begin(), paths.end());
  }
  return allpaths;
}

void CromoServer::SetLogging(
    const std::string& level,
    DBus::Error& error) {  // NOLINT(runtime/references)
  if (SysLogHelperSetLevel(level)) {
    std::string msg(std::string("Invalid Logging Level: ") + level);
    LOG(ERROR) << msg;
    error.set(kDBusInvalidArgs, msg.c_str());
  }
}

void CromoServer::AddModemHandler(ModemHandler* handler) {
  LOG(INFO) << "AddModemHandler(" << handler->vendor_tag() << ")";
  modem_handlers_.push_back(base::WrapUnique(handler));
}

void CromoServer::AddCarrier(Carrier* carrier) {
  carriers_[carrier->name()] = base::WrapUnique(carrier);
}

Carrier* CromoServer::FindCarrierByName(const std::string& name) {
  return carriers_[name].get();
}

Carrier* CromoServer::FindCarrierByCarrierId(carrier_id_t id) {
  for (CarrierMap::iterator i = carriers_.begin(); i != carriers_.end(); ++i) {
    if (i->second && i->second->carrier_id() == id) {
      return i->second.get();
    }
  }
  return nullptr;
}

Carrier* CromoServer::FindCarrierNoOp() {
  if (!carrier_no_op_) {
    carrier_no_op_.reset(new Carrier("no_op_name",
                                     "invalid",
                                     -1,
                                     MM_MODEM_TYPE_GSM,
                                     Carrier::kNone,
                                     nullptr));
  }
  return carrier_no_op_.get();
}
