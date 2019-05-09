// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEMS_SETUP_IIO_CHANNEL_H_
#define MEMS_SETUP_IIO_CHANNEL_H_

#include <base/macros.h>

namespace mems_setup {

class IioDevice;

// The IioChannel represents a channel on an IIO device, for example the
// X axis on an accelerometer.
// Channels can be enabled and/or disabled via this class.
class IioChannel {
 public:
  virtual ~IioChannel() = default;

  // Returns the unique ID of this channel.
  virtual const char* GetId() const = 0;

  // Returns true if this channel is enabled.
  virtual bool IsEnabled() const = 0;

  // Sets this channel's enabled status to |en|.
  // Returns false on failure.
  virtual bool SetEnabled(bool en) = 0;

 protected:
  IioChannel() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IioChannel);
};

}  // namespace mems_setup

#endif  // MEMS_SETUP_IIO_CHANNEL_H_
