// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_DEVICE_EVENT_NOTIFIER_H_
#define MIST_USB_DEVICE_EVENT_NOTIFIER_H_

#include <string>

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop.h>
#include <base/observer_list.h>
#include <gtest/gtest_prod.h>

namespace mist {

class EventDispatcher;
class Udev;
class UdevMonitor;
class UsbDeviceEventObserver;

// A USB device event notifier, which monitors udev events for USB devices and
// notifies registered observers that implement UsbDeviceEventObserver
// interface.
class UsbDeviceEventNotifier : public MessageLoopForIO::Watcher {
 public:
  // Constructs a UsbDeviceEventNotifier object by taking a raw pointer to an
  // EventDispatcher as |dispatcher|. The ownership of |dispatcher| is not
  // transferred, and thus it should outlive this object.
  explicit UsbDeviceEventNotifier(EventDispatcher* dispatcher);

  virtual ~UsbDeviceEventNotifier();

  // Initializes USB device event monitoring such that this object can notify
  // registered observers upon USB device events. Returns true on success.
  bool Initialize();

  // Adds |observer| to the observer list such that |observer| will be notified
  // on USB device events.
  void AddObserver(UsbDeviceEventObserver* observer);

  // Removes |observer| from the observer list such that |observer| will no
  // longer be notified on USB device events.
  void RemoveObserver(UsbDeviceEventObserver* observer);

  // Implements MessageLoopForIO::Watcher.
  virtual void OnFileCanReadWithoutBlocking(int file_descriptor) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int file_descriptor) OVERRIDE;

 private:
  FRIEND_TEST(UsbDeviceEventNotifierTest, ConvertNullToEmptyString);
  FRIEND_TEST(UsbDeviceEventNotifierTest, ConvertIdStringToValue);
  FRIEND_TEST(UsbDeviceEventNotifierTest, OnUsbDeviceEvents);
  FRIEND_TEST(UsbDeviceEventNotifierTest, OnUsbDeviceEventNotAddOrRemove);
  FRIEND_TEST(UsbDeviceEventNotifierTest, OnUsbDeviceEventWithInvalidVendorId);
  FRIEND_TEST(UsbDeviceEventNotifierTest, OnUsbDeviceEventWithInvalidProductId);

  // Returns a string with value of |str| if |str| is not NULL, or an empty
  // string otherwise.
  static std::string ConvertNullToEmptyString(const char* str);

  // Converts a 4-digit hexadecimal ID string (e.g. USB vendor/product ID) into
  // an unsigned 16-bit ID value. Return true on success.
  static bool ConvertIdStringToValue(const std::string& id_string, uint16* id);

  EventDispatcher* const dispatcher_;
  ObserverList<UsbDeviceEventObserver> observer_list_;
  scoped_ptr<Udev> udev_;
  scoped_ptr<UdevMonitor> udev_monitor_;
  int udev_monitor_file_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceEventNotifier);
};

}  // namespace mist

#endif  // MIST_USB_DEVICE_EVENT_NOTIFIER_H_
