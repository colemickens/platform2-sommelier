// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGE_BURN_SERVICE_H_
#define IMAGE_BURN_SERVICE_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <string>

#include "image_burner_impl.h"

namespace imageburn {

// Defined in image_burner.h.
struct ImageBurner;

const int kNumSignals = 2;

enum BurnSignals {
  kSignalBurnFinished,
  kSignalBurnUpdate
};

// Provides a wrapper for exporting ImageBurnerInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
class ImageBurnService : public chromeos::dbus::AbstractDbusService,
                         public SignalSender {
 public:
  ImageBurnService(BurnerImpl* burner_impl);
  virtual ~ImageBurnService();

  // chromeos::dbus::AbstractDbusService implementation.
  virtual const char *service_name() const;
  virtual const char *service_path() const;
  virtual const char *service_interface() const;
  virtual GObject *service_object() const;
  virtual bool Initialize();
  virtual bool Reset();
  virtual bool Shutdown();

  // SignalSender interface.
  virtual void SendFinishedSignal(const char* target_path, bool success,
                                  const char* error_message) OVERRIDE;
  virtual void SendProgressSignal(int64 amount_burnt, int64 total_size,
                                  const char* target_path) OVERRIDE;

  gboolean BurnImageAsync(gchar* from_path, gchar* to_path,
      DBusGMethodInvocation* context);

 protected:
  virtual GMainLoop *main_loop() {
    return main_loop_;
  }
 private:
  void Cleanup();
  void SetError(const std::string& message, GError** error);

  ImageBurner* image_burner_;
  GMainLoop* main_loop_;
  guint signals_[kNumSignals];
  int64 amount_burnt_for_next_signal_;
  bool burning_;
  BurnerImpl* burner_impl_;
};

}  // namespace imageburn

#endif  // IMAGE_BURN_SERVICE_H_
