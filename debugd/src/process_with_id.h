// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PROCESS_WITH_ID_H
#define PROCESS_WITH_ID_H

#include <chromeos/process.h>

namespace debugd {

// @brief Represents a process with an immutable ID.
//
// The ID is random, unguessable, and may be given to other processes. It is a
// null-terminated ASCII string.
class ProcessWithId : public chromeos::ProcessImpl {
 public:
  ProcessWithId();
  bool Init();
  std::string id() const { return id_; }
 private:
  bool generate_id();
  std::string id_;
};

};  // namespace debugd

#endif  // PROCESS_WITH_ID_H
