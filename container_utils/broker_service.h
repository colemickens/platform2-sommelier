// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTAINER_UTILS_BROKER_SERVICE_H_
#define CONTAINER_UTILS_BROKER_SERVICE_H_

#include <limits.h>

#include <map>
#include <memory>

#include <dbus-c++/dbus.h>
#include "org.chromium.PermissionBroker.h"

namespace broker_service {

const char kPermissionBrokerName[] = "org.chromium.PermissionBroker";
const char kPermissionBrokerPath[] = "/org/chromium/PermissionBroker";

struct Connection {
  char path[PATH_MAX];
  size_t path_len;
  int opened_fd;
  Connection() : path_len(0) {}
  bool PathOk() const { return path_len > 0 && path[path_len - 1] == '\0'; }
};

class BrokerService : public org::chromium::PermissionBroker_proxy,
                      public DBus::ObjectProxy {
  // active_requests = (accepted_sockfd, connection)
  std::map<int, std::unique_ptr<Connection>> active_requests;

  // Remove the request for given fd, and close it.
  void RemoveConnection(int fd);

 public:
  BrokerService(DBus::Connection& conn, const char* path, const char* name);

  // Handle a connection.
  void HandleRequest(const std::unique_ptr<Connection>& conn, int sockfd);

  // This runs service for communication with clients.
  void RunService(const char* sockname, int socknamelen);
};

}  // namespace broker_service

#endif  // CONTAINER_UTILS_BROKER_SERVICE_H_
