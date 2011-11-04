// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus-c++/dbus.h>

#include "bindings/org.chromium.debugd.h"

class DebugDaemon : public org::chromium::debugd_adaptor,
                    public DBus::ObjectAdaptor,
                    public DBus::IntrospectableAdaptor {
 public:
  explicit DebugDaemon(DBus::Connection* conn);
  ~DebugDaemon();

  void Init();
  void Run();

  // Public methods below this point are part of the DBus interface presented by
  // this object, and are documented in </share/org.chromium.debugd.xml>.
  virtual std::string PingStart(const int& outfd, const std::string& dest,
                         const std::map<std::basic_string<char>,
                                        DBus::Variant>& options,
                         DBus::Error& error);

  virtual void PingStop(const std::string& handle, DBus::Error& error);

 private:
  DBus::Connection* dbus_;
  DBus::BusDispatcher dispatcher_;
};
