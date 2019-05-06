// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ip_address_store.h"

namespace shill {

// This is a less than comparison so that IPAddress can be stored in a set.
// We do not care about a semantically meaningful comparison. This is
// deterministic, and that's all that matters.
bool IPAddressLTIgnorePrefix::operator () (const IPAddress& lhs,
                                           const IPAddress& rhs) const {
  return lhs.ToString() < rhs.ToString();
}

IPAddressStore::IPAddressStore() = default;

IPAddressStore::~IPAddressStore() = default;

void IPAddressStore::AddUnique(const IPAddress& ip) {
  ip_addresses_.insert(ip);
}

void IPAddressStore::Remove(const IPAddress& ip) {
  ip_addresses_.erase(ip);
}

void IPAddressStore::Clear() {
  ip_addresses_.clear();
}

bool IPAddressStore::Contains(const IPAddress& ip) const {
  return ip_addresses_.find(ip) != ip_addresses_.end();
}

size_t IPAddressStore::Count() const {
  return ip_addresses_.size();
}

bool IPAddressStore::Empty() const {
  return ip_addresses_.empty();
}

}  // namespace shill
