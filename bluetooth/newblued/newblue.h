// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_H_

namespace bluetooth {

// A higher-level API wrapper of the low-level libnewblue C interface.
// This class abstracts away the C interface details of libnewblue and provides
// event handling model that is compatible with libchrome's main loop.
class Newblue {
 public:
  Newblue() = default;
  ~Newblue() = default;

  // Initializes the LE stack (blocking call).
  // Returns true if initialization succeeds, false otherwise.
  // TODO(sonnysasaka): Add real implementations when libnewblue is ready for
  // integration.
  bool Init() { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(Newblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_H_
