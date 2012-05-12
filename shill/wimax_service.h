// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_SERVICE_H_
#define SHILL_WIMAX_SERVICE_H_

#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class WiMaxService : public Service {
 public:
  WiMaxService(ControlInterface *control,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               const WiMaxRefPtr &wimax);
  virtual ~WiMaxService();

  // Inherited from Service.
  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);
  virtual std::string GetStorageIdentifier() const;

 private:
  virtual std::string GetDeviceRpcId(Error *error);

  WiMaxRefPtr wimax_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxService);
};

}  // namespace shill

#endif  // SHILL_WIMAX_SERVICE_H_
