// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SERVER_IMPL_H__
#define CROS_DISKS_SERVER_IMPL_H__

#include "cros-disks-server.h"
#include "disk.h"

#include <string>
#include <vector>

namespace cros_disks {

// The d-bus server for the cros-disks daemon. 
//
// Example Usage:
// 
// DBus::Connection server_conn = DBus::Connection::SystemBus();
// server_conn.request_name("org.chromium.CrosDisks");
// CrosDisksServer* server = new(std::nothrow) CrosDisksServer(server_conn);
// 
// At this point the server should be attached to the main loop.
//
class CrosDisksServer : public org::chromium::CrosDisks_adaptor,
                        public DBus::IntrospectableAdaptor,
                        public DBus::ObjectAdaptor {
 public:
  CrosDisksServer(DBus::Connection& connection);
  virtual ~CrosDisksServer();

  // A method for checking if the daemon is running. Always returns true.
  virtual bool IsAlive(DBus::Error& error);  // NOLINT

  // Unmounts a device when invoked.
  virtual void FilesystemUnmount(const std::vector<std::string>& mountOptions,
                                 DBus::Error& error); // NOLINT

  // Mounts a device when invoked.
  virtual void FilesystemMount(const std::string& nullArgument, 
                               const std::vector<std::string>& mountOptions,
                               DBus::Error& error); // NOLINT

  // Returns a description of every disk device attached to the sytem.
  virtual DBusDisks GetAll(DBus::Error& error); // NOLINT
};
} // namespace cros_disks

#endif // CROS_DISKS_SERVER_IMPL_H__
