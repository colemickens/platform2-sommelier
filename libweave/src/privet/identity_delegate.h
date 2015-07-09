// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_IDENTITY_DELEGATE_H_
#define LIBWEAVE_SRC_PRIVET_IDENTITY_DELEGATE_H_

#include <string>

namespace weave {
namespace privet {

// Interface for an object that can identify ourselves.
class IdentityDelegate {
 public:
  IdentityDelegate() = default;
  virtual ~IdentityDelegate() = default;

  // Returns a unique identifier for this device.
  virtual std::string GetId() const = 0;
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_IDENTITY_DELEGATE_H_
