// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_XSYNC_H_
#define POWER_MANAGER_XSYNC_H_

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "power_manager/xsync_interface.h"

namespace power_manager {

class XSync : public XSyncInterface {
 public:
  XSync();
  virtual ~XSync() {}
  virtual void Init() OVERRIDE;

  virtual bool QueryExtension(int* event_base, int* error_base) OVERRIDE;
  virtual bool Initialize(int* major_version, int* minor_version) OVERRIDE;

  virtual XSyncSystemCounter* ListSystemCounters(int* num_counters OVERRIDE);
  virtual void FreeSystemCounterList(XSyncSystemCounter* counters) OVERRIDE;
  virtual bool QueryCounterInt64(XSyncCounter counter, int64* value) OVERRIDE;
  virtual bool QueryCounter(XSyncCounter counter, XSyncValue* value) OVERRIDE;

  virtual XSyncAlarm CreateAlarm(uint64 mask,
                                 XSyncAlarmAttributes* attrs) OVERRIDE;
  virtual bool DestroyAlarm(XSyncAlarm alarm) OVERRIDE;

  virtual void AddObserver(XEventObserverInterface* observer) OVERRIDE;
  virtual void RemoveObserver(XEventObserverInterface* observer) OVERRIDE;

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(XSync);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_XSYNC_H_
