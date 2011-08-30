// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_card_proxy.h"

using std::string;

namespace shill {

ModemGSMCardProxy::ModemGSMCardProxy(ModemGSMCardProxyListener *listener,
                                     DBus::Connection *connection,
                                     const string &path,
                                     const string &service)
    : proxy_(listener, connection, path, service) {}

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

ModemGSMCardProxy::Proxy::Proxy(ModemGSMCardProxyListener *listener,
                                DBus::Connection *connection,
                                const string &path,
                                const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      listener_(listener) {}

ModemGSMCardProxy::Proxy::~Proxy() {}

}  // namespace shill
