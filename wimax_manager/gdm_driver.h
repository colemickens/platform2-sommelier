// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_GDM_DRIVER_H_
#define WIMAX_MANAGER_GDM_DRIVER_H_

extern "C" {
#include <gct/gctapi.h>
}  // extern "C"

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/weak_ptr.h>

#include "wimax_manager/driver.h"

namespace wimax_manager {

class GdmDevice;
class Network;

class GdmDriver : public Driver,
                  public base::SupportsWeakPtr<GdmDriver> {
 public:
  GdmDriver();
  virtual ~GdmDriver();

  virtual bool Initialize();
  virtual bool Finalize();
  virtual bool GetDevices(std::vector<Device *> *devices);

  bool OpenDevice(GdmDevice *device);
  bool CloseDevice(GdmDevice *device);
  bool GetDeviceStatus(GdmDevice *device);
  bool SetDeviceEAPParameters(GdmDevice *device);
  bool AutoSelectProfileForDevice(GdmDevice *device);
  bool PowerOnDeviceRF(GdmDevice *device);
  bool PowerOffDeviceRF(GdmDevice *device);
  bool GetNetworksForDevice(GdmDevice *device,
                            std::vector<Network *> *networks);
  bool ConnectDeviceToNetwork(GdmDevice *device, const Network *network);
  bool DisconnectDeviceFromNetwork(GdmDevice *device);

 private:
  bool CreateInitialDirectories() const;
  GDEV_ID GetDeviceId(const GdmDevice *device) const;

  APIHAND api_handle_;

  DISALLOW_COPY_AND_ASSIGN(GdmDriver);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_GDM_DRIVER_H_
