// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_MODEM_SWITCHER_H_
#define MIST_USB_MODEM_SWITCHER_H_

#include <string>

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <base/memory/scoped_ptr.h>

#include "mist/usb_device_event_observer.h"

namespace mist {

class Context;
class UsbModemSwitchOperation;

// A USB modem switcher, which initiates a modem switch operation for each
// supported USB device that currently exists on the system, or when a supported
// USB device is added to the system.
class UsbModemSwitcher : public UsbDeviceEventObserver {
 public:
  // Constructs a UsbModemSwitcher object by taking a raw pointer to a Context
  // object as |context|. The ownership of |context| and |modem_info| is not
  // transferred, and thus they should outlive this object.
  explicit UsbModemSwitcher(Context* context);

  ~UsbModemSwitcher();

  // Starts scanning existing USB devices on the system and monitoring new USB
  // devices being added to the system. Initiates a switch operation for each
  // supported device.
  void Start();

 private:
  // Invoked upon the completion of a switch operation where |success| indicates
  // whether the operation completed successfully or not. |operation| is deleted
  // in this callback.
  void OnSwitchOperationCompleted(UsbModemSwitchOperation* operation,
                                  bool success);

  // Implements UsbDeviceEventObserver.
  virtual void OnUsbDeviceAdded(const std::string& sys_path,
                                uint8 bus_number,
                                uint8 device_address,
                                uint16 vendor_id,
                                uint16 product_id) OVERRIDE;
  virtual void OnUsbDeviceRemoved(const std::string& sys_path) OVERRIDE;

  Context* const context_;

  DISALLOW_COPY_AND_ASSIGN(UsbModemSwitcher);
};

}  // namespace mist

#endif  // MIST_USB_MODEM_SWITCHER_H_
