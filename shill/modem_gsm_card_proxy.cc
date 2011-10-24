// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_card_proxy.h"

using std::string;

namespace shill {

ModemGSMCardProxy::ModemGSMCardProxy(ModemGSMCardProxyDelegate *delegate,
                                     DBus::Connection *connection,
                                     const string &path,
                                     const string &service)
    : proxy_(delegate, connection, path, service) {}

ModemGSMCardProxy::~ModemGSMCardProxy() {}

string ModemGSMCardProxy::GetIMEI() {
  return proxy_.GetImei();
}

string ModemGSMCardProxy::GetIMSI() {
  return proxy_.GetImsi();
}

string ModemGSMCardProxy::GetSPN() {
  return proxy_.GetSpn();
}

string ModemGSMCardProxy::GetMSISDN() {
  return proxy_.GetMsIsdn();
}

void ModemGSMCardProxy::EnablePIN(const string &pin, bool enabled) {
  proxy_.EnablePin(pin, enabled);
}

void ModemGSMCardProxy::SendPIN(const string &pin) {
  proxy_.SendPin(pin);
}

void ModemGSMCardProxy::SendPUK(const string &puk, const string &pin) {
  proxy_.SendPuk(puk, pin);
}

void ModemGSMCardProxy::ChangePIN(const string &old_pin,
                                  const string &new_pin) {
  proxy_.ChangePin(old_pin, new_pin);
}

ModemGSMCardProxy::Proxy::Proxy(ModemGSMCardProxyDelegate *delegate,
                                DBus::Connection *connection,
                                const string &path,
                                const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

ModemGSMCardProxy::Proxy::~Proxy() {}

}  // namespace shill
