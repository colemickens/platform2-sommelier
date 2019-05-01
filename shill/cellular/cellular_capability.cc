// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/cellular_capability.h"

#include <memory>

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular/cellular.h"
#include "shill/cellular/cellular_capability_universal.h"
#include "shill/cellular/cellular_capability_universal_cdma.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/property_accessor.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kCellular;
static string ObjectID(CellularCapability* c) {
  return c->cellular()->GetRpcIdentifier();
}
}

const char CellularCapability::kModemPropertyIMSI[] = "imsi";
// All timeout values are in milliseconds
const int CellularCapability::kTimeoutActivate = 300000;
const int CellularCapability::kTimeoutConnect = 90000;
const int CellularCapability::kTimeoutDefault = 5000;
const int CellularCapability::kTimeoutDisconnect = 90000;
const int CellularCapability::kTimeoutEnable = 45000;
const int CellularCapability::kTimeoutGetLocation = 45000;
const int CellularCapability::kTimeoutRegister = 90000;
const int CellularCapability::kTimeoutReset = 90000;
const int CellularCapability::kTimeoutScan = 120000;
const int CellularCapability::kTimeoutSetupLocation = 45000;

// static
std::unique_ptr<CellularCapability> CellularCapability::Create(
    Cellular::Type type, Cellular* cellular, ModemInfo* modem_info) {
  switch (type) {
    case Cellular::kTypeUniversal:
      return std::make_unique<CellularCapabilityUniversal>(cellular,
                                                           modem_info);

    case Cellular::kTypeUniversalCdma:
      return std::make_unique<CellularCapabilityUniversalCdma>(cellular,
                                                               modem_info);

    default:
      NOTREACHED();
      return nullptr;
  }
}

CellularCapability::CellularCapability(Cellular* cellular,
                                       ModemInfo* modem_info)
    : cellular_(cellular), modem_info_(modem_info) {}

CellularCapability::~CellularCapability() {}

void CellularCapability::OnUnsupportedOperation(const char* operation,
                                                Error* error) {
  string message("The ");
  message.append(operation).append(" operation is not supported.");
  Error::PopulateAndLog(FROM_HERE, error, Error::kNotSupported, message);
}

void CellularCapability::Activate(const string& carrier,
                                  Error* error,
                                  const ResultCallback& callback) {
  // Currently activation over the cellular network is not supported using
  // ModemManager. Service activation is currently carried through over
  // non-cellular networks and only the final step of the OTA activation
  // procedure ("automatic activation") is performed.
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::CompleteActivation(Error* error) {
  OnUnsupportedOperation(__func__, error);
}

bool CellularCapability::IsServiceActivationRequired() const {
  return false;
}

void CellularCapability::RegisterOnNetwork(
    const string& /*network_id*/,
    Error* error,
    const ResultCallback& /*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::RequirePIN(const std::string& /*pin*/,
                                    bool /*require*/,
                                    Error* error,
                                    const ResultCallback& /*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::EnterPIN(const string& /*pin*/,
                                  Error* error,
                                  const ResultCallback& /*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::UnblockPIN(const string& /*unblock_code*/,
                                    const string& /*pin*/,
                                    Error* error,
                                    const ResultCallback& /*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::ChangePIN(const string& /*old_pin*/,
                                   const string& /*new_pin*/,
                                   Error* error,
                                   const ResultCallback& /*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::Scan(Error* error,
                              const ResultStringmapsCallback& callback) {
  OnUnsupportedOperation(__func__, error);
}

void CellularCapability::Reset(Error* error,
                               const ResultCallback& /*callback*/) {
  OnUnsupportedOperation(__func__, error);
}

CellularBearer* CellularCapability::GetActiveBearer() const {
  return nullptr;
}

bool CellularCapability::IsActivating() const {
  return false;
}

void CellularCapability::SetupLocation(uint32_t sources,
                                       bool signal_location,
                                       const ResultCallback& callback) {
  Error e(Error::kNotImplemented);
  callback.Run(e);
}

void CellularCapability::GetLocation(const StringCallback& callback) {
  Error e(Error::kNotImplemented);
  callback.Run(string(), e);
}

bool CellularCapability::IsLocationUpdateSupported() {
  return false;
}

void CellularCapability::OnOperatorChanged() {
  SLOG(this, 3) << __func__;
  if (cellular()->service()) {
    UpdateServiceOLP();
  }
}

void CellularCapability::UpdateServiceOLP() {
  SLOG(this, 3) << __func__;
}

}  // namespace shill
