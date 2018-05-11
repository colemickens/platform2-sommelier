// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_H_

#include <memory>

#include "bluetooth/newblued/libnewblue.h"

namespace bluetooth {

// A higher-level API wrapper of the low-level libnewblue C interface.
// This class abstracts away the C interface details of libnewblue and provides
// event handling model that is compatible with libchrome's main loop.
class Newblue {
 public:
  explicit Newblue(std::unique_ptr<LibNewblue> libnewblue);
  virtual ~Newblue() = default;

  // Initializes the LE stack (blocking call).
  // Returns true if initialization succeeds, false otherwise.
  virtual bool Init();

 private:
  std::unique_ptr<LibNewblue> libnewblue_;

  DISALLOW_COPY_AND_ASSIGN(Newblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_H_
