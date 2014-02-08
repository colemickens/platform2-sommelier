// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides helper classes to allow all new DBus methods
// added to Cryptohome to use the "org.freedesktop.DBus.GLib.Async"
// annotation.  By using the annotation, it makes the calls compatible
// with the new chrome/dbus/dbus.h mechanisms.  It will make
// transitioning any new methods more straightforward and provide a means
// to transition existing methods in an incremental fashion.
//
// To transition a method, it will drop OUT_* types from its
// signature, and replace GError with DBusGMethodInvocation
// allowing the handling function to return immediately.
// Any method playing along will PostTask its work directly
// to the mount_thread_.  Upon completion, the method implementation
// will then need to perform a PostTask-equivalent call back to
// the main/DBus thread to issue its reply -- be it success or
// failure.  CryptohomeEventBase is used as a knock-off PostTask
// and the classes in this file provide the glue.

#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/dbus.h>

#include "cryptohome_event_source.h"

namespace cryptohome {

extern const char* kDBusErrorResponseEventType;
extern const char* kDBusNoArgResponseEventType;

struct GErrorDeleter {
  inline void operator()(void* ptr) const {
    g_error_free(static_cast<GError*>(ptr));
  }
};

class DBusErrorResponse : public CryptohomeEventBase {
 public:
  // Takes ownership of both pointers.
  DBusErrorResponse(DBusGMethodInvocation* context, GError* error);
  virtual ~DBusErrorResponse() { }
  virtual const char* GetEventName() const {
    return kDBusErrorResponseEventType;
  }
  virtual void Run() {
    dbus_g_method_return_error (context_, error_.get());
  }
 private:
  // If this event is not serviced, the memory will be leaked.
  DBusGMethodInvocation* context_;
  scoped_ptr<GError, GErrorDeleter> error_;
};

// TODO(wad): Convert this to take a base::Bind which will be
// executed in the main_loop like dbus/bus.h ExportMethod until
// we move to it.
class DBusNoArgResponse : public CryptohomeEventBase {
 public:
  DBusNoArgResponse(DBusGMethodInvocation* context);
  virtual ~DBusNoArgResponse() { }
  virtual const char* GetEventName() const {
    return kDBusNoArgResponseEventType;
  }
  virtual void Run() {
    dbus_g_method_return(context_);
  }
 private:
  // If this event is not serviced, the memory will be leaked.
  DBusGMethodInvocation* context_;
};

}  // namespace cryptohome
