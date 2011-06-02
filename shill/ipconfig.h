// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IPCONFIG_
#define SHILL_IPCONFIG_

#include <string>

#include <base/memory/ref_counted.h>

namespace shill {

class Device;

// IPConfig superclass. Individual IP configuration types will inherit from this
// class.
class IPConfig : public base::RefCounted<IPConfig> {
 public:
  explicit IPConfig(const Device &device);
  virtual ~IPConfig();

  const Device &device() const { return device_; }

  const std::string &GetDeviceName() const;

 private:
  friend class base::RefCounted<IPConfig>;

  const Device &device_;

  DISALLOW_COPY_AND_ASSIGN(IPConfig);
};

}  // namespace shill

#endif  // SHILL_IPCONFIG_
