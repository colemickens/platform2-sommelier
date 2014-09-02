// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_IP_ADDR_H_
#define PEERD_IP_ADDR_H_

#include <netinet/in.h>
#include <sys/socket.h>

namespace peerd {

// We need == defined on a sockaddr_storage, because we have a
// ExportedProperty<vector<sockaddr_storage>> that exposes a list
// of these things to DBus.  Unfortunately, when we call:
//
// ExportedProperty<vector<sockaddr_storage>>::SetValue(v)
//
// the ExportedProperty has to check if the the value has changed with
// the == operator on a vector.  There is no way to pass a comparator
// to vector to tell it how to evaluate element equality, so members
// have to define == themselves.
//
// Rather than pollute the global namespace with our == operator, we
// hide it in a subclass of sockaddr_storage.
struct ip_addr : public sockaddr_storage { };

bool operator==(const ip_addr& v1, const ip_addr& v2);

}  // namespace peerd

#endif  // PEERD_IP_ADDR_H_
