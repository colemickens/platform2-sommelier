// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/mtpd_server_impl.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

namespace mtpd {

MtpdServer::MtpdServer(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, kMtpdServicePath) {
}

MtpdServer::~MtpdServer() {
}

std::vector<std::string> MtpdServer::EnumerateStorage(DBus::Error &error) {
  NOTIMPLEMENTED();
  return std::vector<std::string>();
}

DBusMTPStorage MtpdServer::GetStorageInfo(const std::string& storageName,
                                          DBus::Error &error) {
  NOTIMPLEMENTED();
  return DBusMTPStorage();
}

std::string MtpdServer::OpenStorage(const std::string& storageName,
                                    const std::string& mode,
                                    DBus::Error &error) {
  NOTIMPLEMENTED();
  return std::string();
}

void MtpdServer::CloseStorage(const std::string& handle, DBus::Error &error) {
  NOTIMPLEMENTED();
  return;
}

DBusFileEntries MtpdServer::ReadDirectoryByPath(const std::string& handle,
                                                const std::string& filePath,
                                                DBus::Error &error) {
  NOTIMPLEMENTED();
  return DBusFileEntries();
}

DBusFileEntries MtpdServer::ReadDirectoryById(const std::string& handle,
                                              const uint32_t& fileId,
                                              DBus::Error &error) {
  NOTIMPLEMENTED();
  return DBusFileEntries();
}

std::vector<uint8_t> MtpdServer::ReadFileByPath(const std::string& handle,
                                                const std::string& filePath,
                                                DBus::Error &error) {
  NOTIMPLEMENTED();
  return std::vector<uint8_t>();
}

std::vector<uint8_t> MtpdServer::ReadFileById(const std::string& handle,
                                              const uint32_t& fileId,
                                              DBus::Error &error) {
  NOTIMPLEMENTED();
  return std::vector<uint8_t>();
}

bool MtpdServer::IsAlive(DBus::Error &error) {
  NOTIMPLEMENTED();
  return true;
}

int MtpdServer::GetDeviceEventDescriptor() const {
  NOTIMPLEMENTED();
  return -1;
}

void MtpdServer::ProcessDeviceEvents() {
  NOTIMPLEMENTED();
}

}  // namespace mtpd
