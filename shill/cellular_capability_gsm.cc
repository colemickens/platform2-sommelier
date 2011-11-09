// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <base/logging.h>

#include "shill/cellular.h"
#include "shill/proxy_factory.h"

using std::string;

namespace shill {

CellularCapabilityGSM::CellularCapabilityGSM(Cellular *cellular)
    : CellularCapability(cellular),
      task_factory_(this) {}

void CellularCapabilityGSM::InitProxies() {
  VLOG(2) << __func__;
  card_proxy_.reset(
      proxy_factory()->CreateModemGSMCardProxy(this,
                                               cellular()->dbus_path(),
                                               cellular()->dbus_owner()));
  // TODO(petkov): Move GSM-specific proxy ownership from Cellular to this.
  cellular()->set_modem_gsm_network_proxy(
      proxy_factory()->CreateModemGSMNetworkProxy(cellular(),
                                                  cellular()->dbus_path(),
                                                  cellular()->dbus_owner()));
}

void CellularCapabilityGSM::GetIdentifiers() {
  VLOG(2) << __func__;
  if (cellular()->imei().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_imei(card_proxy_->GetIMEI());
    VLOG(2) << "IMEI: " << cellular()->imei();
  }
  if (cellular()->imsi().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_imsi(card_proxy_->GetIMSI());
    VLOG(2) << "IMSI: " << cellular()->imsi();
  }
  if (cellular()->spn().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    try {
      cellular()->set_spn(card_proxy_->GetSPN());
      VLOG(2) << "SPN: " << cellular()->spn();
    } catch (const DBus::Error e) {
      // Some modems don't support this call so catch the exception explicitly.
      LOG(WARNING) << "Unable to obtain SPN: " << e.what();
    }
  }
  if (cellular()->mdn().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_mdn(card_proxy_->GetMSISDN());
    VLOG(2) << "MSISDN/MDN: " << cellular()->mdn();
  }
}

void CellularCapabilityGSM::RequirePIN(
    const string &pin, bool require, Error */*error*/) {
  VLOG(2) << __func__ << "(" << pin << ", " << require << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::RequirePINTask, pin, require));
}

void CellularCapabilityGSM::RequirePINTask(const string &pin, bool require) {
  VLOG(2) << __func__ << "(" << pin << ", " << require << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->EnablePIN(pin, require);
}

void CellularCapabilityGSM::EnterPIN(const string &pin, Error */*error*/) {
  VLOG(2) << __func__ << "(" << pin << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::EnterPINTask, pin));
}

void CellularCapabilityGSM::EnterPINTask(const string &pin) {
  VLOG(2) << __func__ << "(" << pin << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->SendPIN(pin);
}

void CellularCapabilityGSM::UnblockPIN(
    const string &unblock_code, const string &pin, Error */*error*/) {
  VLOG(2) << __func__ << "(" << unblock_code << ", " << pin << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::UnblockPINTask, unblock_code, pin));
}

void CellularCapabilityGSM::UnblockPINTask(
    const string &unblock_code, const string &pin) {
  VLOG(2) << __func__ << "(" << unblock_code << ", " << pin << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->SendPUK(unblock_code, pin);
}

void CellularCapabilityGSM::ChangePIN(
    const string &old_pin, const string &new_pin, Error */*error*/) {
  VLOG(2) << __func__ << "(" << old_pin << ", " << new_pin << ")";
  // Defer because we may be in a dbus-c++ callback.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(
          &CellularCapabilityGSM::ChangePINTask, old_pin, new_pin));
}

void CellularCapabilityGSM::ChangePINTask(
    const string &old_pin, const string &new_pin) {
  VLOG(2) << __func__ << "(" << old_pin << ", " << new_pin << ")";
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  card_proxy_->ChangePIN(old_pin, new_pin);
}

}  // namespace shill
