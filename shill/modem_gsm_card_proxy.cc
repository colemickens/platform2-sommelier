// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_gsm_card_proxy.h"

#include <base/bind.h>

#include "shill/cellular_error.h"
#include "shill/error.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Callback;
using std::string;

namespace shill {

ModemGSMCardProxy::ModemGSMCardProxy(DBus::Connection *connection,
                                     const string &path,
                                     const string &service)
    : proxy_(connection, path, service) {}

ModemGSMCardProxy::~ModemGSMCardProxy() {}

void ModemGSMCardProxy::GetIMEI(Error *error,
                                const GSMIdentifierCallback &callback,
                                int timeout) {
  scoped_ptr<GSMIdentifierCallback> cb(new GSMIdentifierCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetImei(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMCardProxy::GetIMSI(Error *error,
                                const GSMIdentifierCallback &callback,
                                int timeout) {
  scoped_ptr<GSMIdentifierCallback> cb(new GSMIdentifierCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetImsi(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMCardProxy::GetSPN(Error *error,
                               const GSMIdentifierCallback &callback,
                               int timeout) {
  scoped_ptr<GSMIdentifierCallback> cb(new GSMIdentifierCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetSpn(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMCardProxy::GetMSISDN(Error *error,
                                  const GSMIdentifierCallback &callback,
                                  int timeout) {
  scoped_ptr<GSMIdentifierCallback> cb(new GSMIdentifierCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.GetMsIsdn(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMCardProxy::EnablePIN(const string &pin, bool enabled,
                                  Error *error,
                                  const ResultCallback &callback,
                                  int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.EnablePin(pin, enabled, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMCardProxy::SendPIN(const string &pin,
                                Error *error,
                                const ResultCallback &callback,
                                int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.SendPin(pin, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMCardProxy::SendPUK(const string &puk, const string &pin,
                                Error *error,
                                const ResultCallback &callback,
                                int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.SendPuk(puk, pin, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

void ModemGSMCardProxy::ChangePIN(const string &old_pin,
                                  const string &new_pin,
                                  Error *error,
                                  const ResultCallback &callback,
                                  int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    SLOG(DBus, 2) << __func__;
    proxy_.ChangePin(old_pin, new_pin, cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      CellularError::FromDBusError(e, error);
  }
}

uint32 ModemGSMCardProxy::EnabledFacilityLocks() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.EnabledFacilityLocks();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return 0;  // Make the compiler happy.
  }
}

ModemGSMCardProxy::Proxy::Proxy(DBus::Connection *connection,
                                const string &path,
                                const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

void ModemGSMCardProxy::Proxy::GetIdCallback(const string &id,
                                             const DBus::Error &dberror,
                                             void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<GSMIdentifierCallback> callback(
      reinterpret_cast<GSMIdentifierCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(id, error);
}

void ModemGSMCardProxy::Proxy::GetImeiCallback(const string &imei,
                                               const DBus::Error &dberror,
                                               void *data) {
  SLOG(DBus, 2) << __func__;
  GetIdCallback(imei, dberror, data);
}

void ModemGSMCardProxy::Proxy::GetImsiCallback(const string &imsi,
                                               const DBus::Error &dberror,
                                               void *data) {
  SLOG(DBus, 2) << __func__;
  GetIdCallback(imsi, dberror, data);
}

void ModemGSMCardProxy::Proxy::GetSpnCallback(const string &spn,
                                              const DBus::Error &dberror,
                                              void *data) {
  SLOG(DBus, 2) << __func__;
  GetIdCallback(spn, dberror, data);
}

void ModemGSMCardProxy::Proxy::GetMsIsdnCallback(const string &msisdn,
                                                 const DBus::Error &dberror,
                                                 void *data) {
  SLOG(DBus, 2) << __func__;
  GetIdCallback(msisdn, dberror, data);
}

void ModemGSMCardProxy::Proxy::PinCallback(const DBus::Error &dberror,
                                           void *data) {
  SLOG(DBus, 2) << __func__;
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error error;
  CellularError::FromDBusError(dberror, &error);
  callback->Run(error);
}

void ModemGSMCardProxy::Proxy::EnablePinCallback(const DBus::Error &dberror,
                                                 void *data) {
  SLOG(DBus, 2) << __func__;
  PinCallback(dberror, data);
}

void ModemGSMCardProxy::Proxy::SendPinCallback(const DBus::Error &dberror,
                                               void *data) {
  SLOG(DBus, 2) << __func__;
  PinCallback(dberror, data);
}

void ModemGSMCardProxy::Proxy::SendPukCallback(const DBus::Error &dberror,
                                               void *data) {
  SLOG(DBus, 2) << __func__;
  PinCallback(dberror, data);
}

void ModemGSMCardProxy::Proxy::ChangePinCallback(const DBus::Error &dberror,
                                                 void *data) {
  SLOG(DBus, 2) << __func__;
  PinCallback(dberror, data);
}

ModemGSMCardProxy::Proxy::~Proxy() {}

}  // namespace shill
