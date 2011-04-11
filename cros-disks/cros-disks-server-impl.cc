// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks-server-impl.h"

#include "disk.h"

#include <sys/mount.h>

namespace cros_disks {

// TODO(rtc): this should probably be a flag.
static const char* kServicePath = "/org/chromium/CrosDisks";

CrosDisksServer::CrosDisksServer(DBus::Connection& connection)  
    : DBus::ObjectAdaptor(connection, kServicePath) { }

CrosDisksServer::~CrosDisksServer() { }

bool CrosDisksServer::IsAlive(DBus::Error& error) {  // NOLINT
  return true;
}

void CrosDisksServer::FilesystemMount(
    const std::string& nullArgument, 
    const std::vector<std::string>& mountOptions,
    DBus::Error& error) {  // NOLINT

  return;
}

void CrosDisksServer::FilesystemUnmount(
    const std::vector<std::string>& mountOptions,
    DBus::Error& error) {  // NOLINT

  return;
}

DBusDisks CrosDisksServer::GetAll(DBus::Error& error) { // NOLINT
  DBusDisks v;
  return v;
}

} // namespace cros_disks
