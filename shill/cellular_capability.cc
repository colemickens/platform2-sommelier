// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular.h"
#include "shill/error.h"
#include "shill/property_accessor.h"
#include "shill/scope_logger.h"

using base::Closure;
using std::string;

namespace shill {

const char CellularCapability::kModemPropertyIMSI[] = "imsi";
const char CellularCapability::kModemPropertyState[] = "State";
// All timeout values are in milliseconds
const int CellularCapability::kTimeoutActivate = 120000;
const int CellularCapability::kTimeoutConnect = 45000;
const int CellularCapability::kTimeoutDefault = 5000;
const int CellularCapability::kTimeoutEnable = 15000;
const int CellularCapability::kTimeoutRegister = 90000;
const int CellularCapability::kTimeoutScan = 120000;

CellularCapability::CellularCapability(Cellular *cellular,
                                       ProxyFactory *proxy_factory)
    : cellular_(cellular),
      proxy_factory_(proxy_factory),
      allow_roaming_(false) {
  HelpRegisterDerivedBool(flimflam::kCellularAllowRoamingProperty,
                          &CellularCapability::GetAllowRoaming,
                          &CellularCapability::SetAllowRoaming);
}

CellularCapability::~CellularCapability() {}

void CellularCapability::HelpRegisterDerivedBool(
    const string &name,
    bool(CellularCapability::*get)(Error *error),
    void(CellularCapability::*set)(const bool &value, Error *error)) {
  cellular()->mutable_store()->RegisterDerivedBool(
      name,
      BoolAccessor(
          new CustomAccessor<CellularCapability, bool>(this, get, set)));
}

void CellularCapability::SetAllowRoaming(const bool &value, Error */*error*/) {
  SLOG(Cellular, 2) << __func__
                    << "(" << allow_roaming_ << "->" << value << ")";
  if (allow_roaming_ == value) {
    return;
  }
  allow_roaming_ = value;
  // Use AllowRoaming() instead of allow_roaming_ in order to
  // incorporate provider preferences when evaluating if a disconnect
  // is required.
  if (!AllowRoaming() &&
      GetRoamingStateString() == flimflam::kRoamingStateRoaming) {
    Error error;
    cellular()->Disconnect(&error);
  }
  cellular()->adaptor()->EmitBoolChanged(
      flimflam::kCellularAllowRoamingProperty, value);
}

void CellularCapability::RunNextStep(CellularTaskList *tasks) {
  CHECK(!tasks->empty());
  SLOG(Cellular, 2) << __func__ << ": " << tasks->size() << " remaining tasks";
  Closure task = (*tasks)[0];
  tasks->erase(tasks->begin());
  cellular()->dispatcher()->PostTask(task);
}

void CellularCapability::StepCompletedCallback(
    const ResultCallback &callback,
    bool ignore_error,
    CellularTaskList *tasks,
    const Error &error) {
  if ((ignore_error || error.IsSuccess()) && !tasks->empty()) {
    RunNextStep(tasks);
    return;
  }
  delete tasks;
  callback.Run(error);
}

void CellularCapability::OnUnsupportedOperation(
    const char *operation,
    Error *error) {
  string message("The ");
  message.append(operation).append(" operation is not supported.");
  Error::PopulateAndLog(error, Error::kNotSupported, message);
}

void CellularCapability::RegisterOnNetwork(
    const string &/*network_id*/,
    Error *error, const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::RequirePIN(const std::string &/*pin*/,
                                    bool /*require*/,
                                    Error *error,
                                    const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::EnterPIN(const string &/*pin*/,
                                  Error *error,
                                  const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::UnblockPIN(const string &/*unblock_code*/,
                                    const string &/*pin*/,
                                    Error *error,
                                    const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::ChangePIN(const string &/*old_pin*/,
                                   const string &/*new_pin*/,
                                   Error *error,
                                   const ResultCallback &/*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::Scan(Error *error,
                              const ResultCallback &callback) {
  OnUnsupportedOperation(__func__, error);
}

}  // namespace shill
