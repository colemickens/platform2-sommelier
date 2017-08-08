// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements DBus functionality.
// Because hammerd is not a daemon, it can only send signals to other
// processes or call methods provided by others.

#ifndef HAMMERD_DBUS_WRAPPER_H_
#define HAMMERD_DBUS_WRAPPER_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/ptr_util.h>
#include <base/memory/ref_counted.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace hammerd {

class DBusWrapperInterface {
 public:
  virtual ~DBusWrapperInterface() = default;

  // Send a signal.
  virtual void SendSignal(dbus::Signal* signal) = 0;
  virtual void SendSignal(const std::string& signal_name) = 0;
};

class DBusWrapper : public DBusWrapperInterface {
 public:
  DBusWrapper();
  virtual ~DBusWrapper() = default;
  void SendSignal(dbus::Signal* signal) override;
  void SendSignal(const std::string& signal_name) override;

 protected:
  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusWrapper);
};

}  // namespace hammerd
#endif  // HAMMERD_DBUS_WRAPPER_H_
