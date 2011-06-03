// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IPCONFIG_
#define SHILL_IPCONFIG_

#include <string>
#include <vector>

#include <base/memory/ref_counted.h>

namespace shill {

class Device;

// IPConfig superclass. Individual IP configuration types will inherit from this
// class.
class IPConfig : public base::RefCounted<IPConfig> {
 public:
  struct Properties {
    Properties() : subnet_cidr(0), mtu(0) {}

    std::string address;
    int subnet_cidr;
    std::string broadcast_address;
    std::string gateway;
    std::vector<std::string> dns_servers;
    std::string domain_name;
    std::vector<std::string> domain_search;
    int mtu;
  };

  explicit IPConfig(const Device &device);
  virtual ~IPConfig();

  const Device &device() const { return device_; }
  const std::string &GetDeviceName() const;

  // Updates the IP configuration properties and notifies registered listeners
  // about the event.
  void UpdateProperties(const Properties &properties);

  const Properties &properties() const { return properties_; }

  // Request or renew IP configuration. Return true on success, false
  // otherwise. The default implementation always returns false indicating a
  // failure.
  virtual bool Request();
  virtual bool Renew();

 private:
  const Device &device_;
  Properties properties_;

  DISALLOW_COPY_AND_ASSIGN(IPConfig);
};

}  // namespace shill

#endif  // SHILL_IPCONFIG_
