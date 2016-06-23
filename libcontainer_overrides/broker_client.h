// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCONTAINER_OVERRIDES_BROKER_CLIENT_H_
#define LIBCONTAINER_OVERRIDES_BROKER_CLIENT_H_

#define BROKER_SOCKET_PATH        "/run/broker_service/adb"
#define BROKER_SOCKET_PATH_LEN    (sizeof(BROKER_SOCKET_PATH) - 1)

#include <brillo/brillo_export.h>

// Acquires a socket fd to communicate with broker_service. sockname is the
// null terminated filesystem pathname for unix domain socket over which
// we should talk to broker_service, socknamelen is the pathname length.
// Returns client side socket fd on success, -1 otherwise (error flags set
// except when socknamelen exceeds max allowable length).
BRILLO_EXPORT int ConnectToBroker(const char *sockname, int socknamelen);

// Requests a file descriptor to a USB device from broker_service. Returns
// fd given by broker_service on success, -1 otherwise (error flags set).
BRILLO_EXPORT int OpenWithPermissions(int sockfd, const char *path);

#endif  // LIBCONTAINER_OVERRIDES_BROKER_CLIENT_H_
