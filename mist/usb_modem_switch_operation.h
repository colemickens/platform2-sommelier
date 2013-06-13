// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_MODEM_SWITCH_OPERATION_H_
#define MIST_USB_MODEM_SWITCH_OPERATION_H_

#include <string>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/compiler_specific.h>
#include <base/memory/scoped_ptr.h>

#include "mist/usb_device_event_observer.h"

namespace mist {

class Context;
class UsbBulkTransfer;
class UsbDevice;
class UsbModemInfo;
class UsbTransfer;

// A USB modem switch operation, which switches a USB modem from the mass
// storage mode to the modem mode. The whole operation involves the following
// tasks:
// 1. Open the USB modem device, find and claim the mass storage interface of
//    the modem.
// 2. Initiate a bulk transfer of a (or multiple) special USB message(s) to
//    the mass storage endpoint of the modem.
// 3. Once the USB message(s) is sent, the modem is expected to disconnect from
//    the USB bus and then reconnect to the bus after it has been switched to
//    the modem mode.
//
// As mist may run multiple modem switch operations concurrently, in order to
// maximize the overall concurrency, the modem switch operation is broken up
// into the aforementioned tasks and each task is scheduled to execute in the
// message loop via EventDispatcher.
class UsbModemSwitchOperation : public UsbDeviceEventObserver {
 public:
  typedef base::Callback<void(UsbModemSwitchOperation* operation, bool success)>
      CompletionCallback;

  // Constructs a UsbModemSwitchOperation object by taking a raw pointer to a
  // Context object as |context|, a UsbDevice object as |device| that
  // corresponds to the device being switched, the device sysfs path as
  // |device_sys_path|, and a raw pointer to a UsbModemInfo object as
  // |modem_info| that contains the information about how to switch the device
  // to the modem mode. The ownership of |context| and |modem_info| is not
  // transferred, and thus they should outlive this object. The ownership of
  // |device| is transferred.
  UsbModemSwitchOperation(Context* context,
                          UsbDevice* device,
                          const std::string& device_sys_path,
                          const UsbModemInfo* modem_info);

  ~UsbModemSwitchOperation();

  // Starts the modem switch operation. Upon the completion of the operation,
  // the completion callback |completion_callback| is invoked with the status
  // of the operation.
  void Start(const CompletionCallback& completion_callback);

 private:
  typedef void (UsbModemSwitchOperation::*Task)();

  // Schedules the next task in the message loop for execution. At most one
  // pending task is allowed at any time.
  void ScheduleTask(Task task);

  // Completes the operation, which invokes the completion callback with the
  // status of the operation as |success|. The completion callback may delete
  // this object, so this object should not be accessed after this method
  // returns.
  void Complete(bool success);

  // Closes the device.
  void CloseDevice();

  // Opens the device and claims the mass storage interface on the device.
  void OpenDeviceAndClaimMassStorageInterface();

  // Sends a (or multiple) special USB message(s) to the mass storage endpoint
  // of the device.
  void SendMessageToMassStorageEndpoint();

  // Invoked when this switcher times out waiting for the device to reconnect to
  // the bus, after the special USB message(s) is sent to the mass storage
  // endpoint by SendMessageToMassStorageEndpoint().
  void OnReconnectTimeout();

  // Invoked upon the completion of the transfer of the special USB message(s).
  void OnUsbMessageTransferred(UsbTransfer* transfer);

  // Implements UsbDeviceEventObserver.
  virtual void OnUsbDeviceAdded(const std::string& sys_path,
                                uint8 bus_number,
                                uint8 device_address,
                                uint16 vendor_id,
                                uint16 product_id) OVERRIDE;
  virtual void OnUsbDeviceRemoved(const std::string& sys_path) OVERRIDE;

  Context* const context_;
  scoped_ptr<UsbDevice> device_;
  std::string device_sys_path_;
  const UsbModemInfo* const modem_info_;
  CompletionCallback completion_callback_;
  bool interface_claimed_;
  uint8 interface_number_;
  uint8 endpoint_address_;
  scoped_ptr<UsbBulkTransfer> bulk_transfer_;
  base::CancelableClosure pending_task_;
  base::CancelableClosure reconnect_timeout_callback_;

  DISALLOW_COPY_AND_ASSIGN(UsbModemSwitchOperation);
};

}  // namespace mist

#endif  // MIST_USB_MODEM_SWITCH_OPERATION_H_
