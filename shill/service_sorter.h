// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICE_SORTER_
#define SERVICE_SORTER_

#include <vector>

#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class Manager;

// This is a closure used by the Manager for STL sorting of the
// Service array.  We pass instances of this object to STL sort(),
// which in turn will call the selected function in the Manager to
// compare two Service objects at a time.
class ServiceSorter {
 public:
  explicit ServiceSorter(const std::vector<Technology::Identifier> &tech_order)
      : technology_order_(tech_order) {}
  bool operator() (ServiceRefPtr a, ServiceRefPtr b) {
    const char *reason;
    return Service::Compare(a, b, technology_order_, &reason);
  }

 private:
  const std::vector<Technology::Identifier> &technology_order_;
  // We can't DISALLOW_COPY_AND_ASSIGN since this is passed by value to STL sort
};

}  // namespace shill

#endif  // SERVICE_SORTER_
