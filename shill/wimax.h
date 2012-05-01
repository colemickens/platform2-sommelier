// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_H_
#define SHILL_WIMAX_H_

#include "shill/device.h"

namespace shill {

class WiMax : public Device {
 public:
  WiMax(ControlInterface *control,
        EventDispatcher *dispatcher,
        Metrics *metrics,
        Manager *manager,
        const std::string &link_name,
        int interface_index);

  virtual ~WiMax();

  virtual void Start(Error *error, const EnabledStateChangedCallback &callback);
  virtual void Stop(Error *error, const EnabledStateChangedCallback &callback);

  virtual bool TechnologyIs(const Technology::Identifier type) const;

  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);

 private:
  DISALLOW_COPY_AND_ASSIGN(WiMax);
};

}  // namespace shill

#endif  // SHILL_WIMAX_H_
