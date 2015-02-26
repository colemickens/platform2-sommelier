// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHE_H_
#define PSYCHE_PSYCHE_H_

#include <map>
#include <string>

#include <base/macros.h>

namespace psyche {

struct ServiceInfo {
  bool is_running;
  bool is_ephemeral;
};

class Psyche {
 public:
  Psyche();
  virtual ~Psyche();

  // This method initializes psyche data strucutures, and must be called before
  // any other method is called.
  virtual void Init();

  // This method registers a service with psyche.
  // Returns true if a new service was added, else false.
  virtual bool AddService(const std::string& service_name,
                          bool is_running,
                          bool is_ephemeral);

  // This method gets the service status struct from psyche's internal list.
  // If succesful, |status| is populated with the status of the given service,
  // and true is returned.
  virtual bool FindServiceStatus(const std::string& service_name,
                                 ServiceInfo* status);

 protected:
  friend class PsycheTest;

 private:
  std::map<std::string, ServiceInfo> service_info_map_;
  DISALLOW_COPY_AND_ASSIGN(Psyche);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHE_H_
