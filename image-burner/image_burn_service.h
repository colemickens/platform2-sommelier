// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGE_BURN_SERVICE_H_
#define IMAGE_BURN_SERVICE_H_

#include <base/basictypes.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <string>

namespace imageburn {

// Defined in image_burner.h.
struct ImageBurner;

class ImageBurnService;

struct BurnArguments {
  std::string from_path;
  std::string to_path;
  ImageBurnService* service;
};

const int kNumSignals = 2;

enum BurnSignals {
  kSignalBurnFinished,
  kSignalBurnUpdate
};

// Provides a wrapper for exporting ImageBurnerInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
class ImageBurnService : public chromeos::dbus::AbstractDbusService {
 public:
  ImageBurnService();
  virtual ~ImageBurnService();

  // chromeos::dbus::AbstractDbusService implementation.
  virtual const char *service_name() const;
  virtual const char *service_path() const;
  virtual const char *service_interface() const;
  virtual GObject *service_object() const;
  virtual bool Initialize();
  virtual bool Reset();
  virtual bool Shutdown();

  gboolean BurnImageAsync(gchar* from_path, gchar* to_path,
      DBusGMethodInvocation* context);

 protected:
  virtual GMainLoop *main_loop() {
    return main_loop_;
  }
 private:
  void Cleanup();
  void SetError(const std::string& message, GError** error);
  bool ValidateTargetPath(const char* target_path, std::string* error);
  bool BurnImageImpl(const char* from_path, const char* to_path,
      std::string* error);
  static gboolean BurnImageTimeoutCallback(gpointer data);
  void InitiateBurning(const char* from_path, const char* to_path);
  bool DoBurn(const char* from_path, const char* to_path, std::string* err);
  void ProcessFileError(const std::string& error, const std::string& path,
                        std::string* error_message);
  void SendProgressSignal(int64 amount_burnt,
                          int64 total_size,
                          const char* target_path);
  void SendFinishedSignal(const char* target_path,
                          bool success,
                          const char* error);
  int64 GetTotalImageSize(FILE* file);

  ImageBurner* image_burner_;
  GMainLoop* main_loop_;
  guint signals_[kNumSignals];
  bool shutting_down_;
  bool burning_;
  std::string from_path_;
  std::string to_path_;
  int devices_to_unmount_count_;
};

}  // namespace imageburn

#endif  // IMAGE_BURN_SERVICE_H_

