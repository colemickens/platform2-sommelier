// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_UDEV_CONTROLLER_H_
#define POWER_MANAGER_UDEV_CONTROLLER_H_

#include <glib.h>
#include <libudev.h>

#include <string>

#include "base/basictypes.h"

namespace power_manager {

class UdevDelegate {
 public:
  virtual ~UdevDelegate() {}
  virtual void Run(GIOChannel* source, GIOCondition condition) = 0;
};

class UdevController {
 public:
  UdevController(UdevDelegate* delegate, const std::string& subsystem);
  ~UdevController();

  bool Init();

 private:
  static gboolean EventHandler(GIOChannel* source,
                               GIOCondition condition,
                               gpointer data);

  // The name of the DRM subsystem we are listening to.
  std::string subsystem_;

  // Delegate for udev event; not owned.
  UdevDelegate* delegate_;

  struct udev_monitor* monitor_;
  struct udev* udev_;
  DISALLOW_COPY_AND_ASSIGN(UdevController);
};

} // namespace power_manager

#endif  // POWER_MANAGER_UDEV_CONTROLLER_H_
