// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_NETWORK_H_
#define WIMAX_MANAGER_NETWORK_H_

#include <string>

#include <base/basictypes.h>

namespace wimax_manager {

class Network {
 public:
  Network(uint32 id, const std::string& name);
  ~Network();

  uint32 id() const { return id_; }
  const std::string &name() const { return name_; }

 private:
  uint32 id_;
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(Network);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_NETWORK_H_
