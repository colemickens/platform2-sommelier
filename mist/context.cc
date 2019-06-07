// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/context.h"

#include <brillo/udev/udev.h>

#include "mist/config_loader.h"
#include "mist/event_dispatcher.h"
#include "mist/metrics.h"
#include "mist/usb_device_event_notifier.h"
#include "mist/usb_manager.h"

namespace mist {

Context::Context() = default;

Context::~Context() = default;

bool Context::Initialize() {
  metrics_.reset(new Metrics());
  CHECK(metrics_);

  config_loader_.reset(new ConfigLoader());
  CHECK(config_loader_);
  if (!config_loader_->LoadDefaultConfig()) {
    LOG(ERROR) << "Could not load default config file.";
    return false;
  }

  event_dispatcher_.reset(new EventDispatcher());
  CHECK(event_dispatcher_);

  udev_.reset(new brillo::Udev());
  CHECK(udev_);
  if (!udev_->Initialize()) {
    LOG(ERROR) << "Could not create udev library context.";
    return false;
  }

  usb_device_event_notifier_.reset(
      new UsbDeviceEventNotifier(event_dispatcher_.get(), udev_.get()));
  CHECK(usb_device_event_notifier_);
  if (!usb_device_event_notifier_->Initialize()) {
    LOG(ERROR) << "Could not initialize USB device event notification.";
    return false;
  }

  usb_manager_.reset(new UsbManager(event_dispatcher_.get()));
  if (!usb_manager_->Initialize()) {
    LOG(ERROR) << "Could not initialize USB manager: " << usb_manager_->error();
    return false;
  }

  return true;
}

}  // namespace mist
