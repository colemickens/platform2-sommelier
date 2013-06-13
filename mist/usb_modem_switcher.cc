// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/usb_modem_switcher.h"

#include <base/bind.h>
#include <base/stringprintf.h>

#include "mist/config_loader.h"
#include "mist/context.h"
#include "mist/event_dispatcher.h"
#include "mist/proto_bindings/usb_modem_info.pb.h"
#include "mist/usb_device.h"
#include "mist/usb_device_event_notifier.h"
#include "mist/usb_manager.h"
#include "mist/usb_modem_switch_operation.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::string;

namespace mist {

// TODO(benchan): Add unit tests for UsbModemSwitcher.

UsbModemSwitcher::UsbModemSwitcher(Context* context) : context_(context) {
  CHECK(context_);
}

UsbModemSwitcher::~UsbModemSwitcher() {
  context_->usb_device_event_notifier()->RemoveObserver(this);
}

void UsbModemSwitcher::Start() {
  context_->usb_device_event_notifier()->AddObserver(this);
  context_->usb_device_event_notifier()->ScanExistingDevices();
}

void UsbModemSwitcher::OnSwitchOperationCompleted(
    UsbModemSwitchOperation* operation,
    bool success) {
  CHECK(operation);
  delete operation;
}

void UsbModemSwitcher::OnUsbDeviceAdded(const string& sys_path,
                                        uint8 bus_number,
                                        uint8 device_address,
                                        uint16 vendor_id,
                                        uint16 product_id) {
  const UsbModemInfo* modem_info =
      context_->config_loader()->GetUsbModemInfo(vendor_id, product_id);
  if (!modem_info)
    return;  // Ignore an unsupported device.

  UsbDevice* device = context_->usb_manager()->GetDevice(
      bus_number, device_address, vendor_id, product_id);
  if (!device) {
    LOG(ERROR) << StringPrintf("Could not find USB device '%s' "
                               "(Bus %03d Address %03d ID %04x:%04x).",
                               sys_path.c_str(),
                               bus_number,
                               device_address,
                               vendor_id,
                               product_id);
    return;
  }

  UsbModemSwitchOperation* operation =
      new UsbModemSwitchOperation(context_, device, sys_path, modem_info);
  CHECK(operation);

  // The operation object will be deleted in OnSwitchOperationCompleted().
  operation->Start(
      Bind(&UsbModemSwitcher::OnSwitchOperationCompleted, Unretained(this)));
}

void UsbModemSwitcher::OnUsbDeviceRemoved(const string& sys_path) {
  // UsbModemSwitcher does not need to handle device removal.
}

}  // namespace mist
