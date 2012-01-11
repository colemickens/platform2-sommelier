// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability.h"

#include "shill/adaptor_interfaces.h"
#include "shill/cellular.h"
#include "shill/error.h"

using std::string;

namespace shill {

CellularCapability::CellularCapability(Cellular *cellular)
    : cellular_(cellular),
      proxy_factory_(cellular->proxy_factory()) {}

CellularCapability::~CellularCapability() {}

EventDispatcher *CellularCapability::dispatcher() const {
  return cellular()->dispatcher();
}

void CellularCapability::Activate(const string &/*carrier*/, Error *error) {
  Error::PopulateAndLog(error, Error::kInvalidArguments,
                        "Activation not supported.");
}

void CellularCapability::Register() {}

void CellularCapability::RegisterOnNetwork(
    const string &/*network_id*/, Error *error) {
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Network registration not supported.");
}

void CellularCapability::RequirePIN(
    const string &/*pin*/, bool /*require*/, ReturnerInterface *returner) {
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "RequirePIN not supported.");
  returner->ReturnError(error);
}

void CellularCapability::EnterPIN(const string &/*pin*/,
                                  ReturnerInterface *returner) {
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "EnterPIN not supported.");
  returner->ReturnError(error);
}

void CellularCapability::UnblockPIN(const string &/*unblock_code*/,
                                    const string &/*pin*/,
                                    ReturnerInterface *returner) {
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "UnblockPIN not supported.");
  returner->ReturnError(error);
}

void CellularCapability::ChangePIN(const string &/*old_pin*/,
                                   const string &/*new_pin*/,
                                   ReturnerInterface *returner) {
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "ChangePIN not supported.");
  returner->ReturnError(error);
}

void CellularCapability::Scan(Error *error) {
  Error::PopulateAndLog(
      error, Error::kNotSupported, "Network scanning not supported.");
}

}  // namespace shill
