// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_SORTER_H_
#define SHILL_SERVICE_SORTER_H_

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
  ServiceSorter(Manager* manager,
                bool compare_connectivity_state,
                const std::vector<Technology>& tech_order)
      : manager_(manager),
        compare_connectivity_state_(compare_connectivity_state),
        technology_order_(tech_order) {}
  bool operator() (ServiceRefPtr a, ServiceRefPtr b) {
    const char* reason;
    return Service::Compare(manager_, a, b, compare_connectivity_state_,
                            technology_order_, &reason);
  }

 private:
  Manager* manager_;
  const bool compare_connectivity_state_;
  const std::vector<Technology>& technology_order_;
  // We can't DISALLOW_COPY_AND_ASSIGN since this is passed by value to STL
  // sort.
};

}  // namespace shill

#endif  // SHILL_SERVICE_SORTER_H_
