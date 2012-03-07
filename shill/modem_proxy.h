// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_PROXY_
#define SHILL_MODEM_PROXY_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem.h"
#include "shill/modem_proxy_interface.h"

namespace shill {

// A proxy to ModemManager.Modem.
class ModemProxy : public ModemProxyInterface {
 public:
  // Constructs a ModemManager.Modem DBus object proxy at |path| owned by
  // |service|.
  ModemProxy(DBus::Connection *connection,
             const std::string &path,
             const std::string &service);
  virtual ~ModemProxy();

  // Inherited from ModemProxyInterface.
  virtual void Enable(bool enable, Error *error,
                      const ResultCallback &callback, int timeout);
  virtual void Disconnect(Error *error, const ResultCallback &callback,
                          int timeout);
  virtual void GetModemInfo(Error *error, const ModemInfoCallback &callback,
                            int timeout);

  virtual void set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback);

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

    void set_state_changed_callback(
        const ModemStateChangedSignalCallback &callback);

   private:
    // Signal callbacks inherited from ModemManager::Modem_proxy.
    virtual void StateChanged(
        const uint32 &old, const uint32 &_new, const uint32 &reason);

    // Method callbacks inherited from ModemManager::Modem_proxy.
    virtual void EnableCallback(const DBus::Error &dberror, void *data);
    virtual void GetInfoCallback(const ModemHardwareInfo &info,
                                 const DBus::Error &dberror, void *data);
    virtual void DisconnectCallback(const DBus::Error &dberror, void *data);

    ModemStateChangedSignalCallback state_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_PROXY_
