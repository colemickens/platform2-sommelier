// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_MANAGER_H_
#define MIST_USB_MANAGER_H_

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <base/memory/scoped_vector.h>
#include <base/message_loop.h>

#include "mist/usb_error.h"

struct libusb_context;

namespace mist {

class EventDispatcher;
class UsbDevice;

// A USB manager for managing a USB session created by libusb 1.0.
class UsbManager : public MessageLoopForIO::Watcher {
 public:
  // Constructs a UsbManager object by taking a raw pointer to an
  // EventDispatcher as |dispatcher|. The ownership of |dispatcher| is not
  // transferred, and thus it should outlive this object.
  explicit UsbManager(EventDispatcher* dispatcher);

  virtual ~UsbManager();

  // Initializes a USB session via libusb. Returns true on success.
  bool Initialize();

  // Sets the debug level of libusb to |level|.
  void SetDebugLevel(int level);

  // Gets the list of USB devices currently attached to the system. Returns true
  // on success. |devices| is always cleared before being updated. The returned
  // UsbDevice objects become invalid, and thus should not be held, beyond the
  // lifetime of this object.
  bool GetDevices(ScopedVector<UsbDevice>* devices);

  const UsbError& error() const { return error_; }

 private:
  static void OnPollFileDescriptorAdded(int file_descriptor,
                                        short events,  // NOLINT
                                        void* user_data);
  static void OnPollFileDescriptorRemoved(int file_descriptor, void* user_data);

  // Starts watching the file descriptors for libusb events. Returns true on
  // success.
  bool StartWatchingPollFileDescriptors();

  // Handles libusb events in non-blocking mode.
  void HandleEventsNonBlocking();

  // Implements MessageLoopForIO::Watcher.
  virtual void OnFileCanReadWithoutBlocking(int file_descriptor) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int file_descriptor) OVERRIDE;

  EventDispatcher* const dispatcher_;
  libusb_context* context_;
  UsbError error_;

  DISALLOW_COPY_AND_ASSIGN(UsbManager);
};

}  // namespace mist

#endif  // MIST_USB_MANAGER_H_
