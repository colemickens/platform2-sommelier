// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_IMAGELOADER_H_
#define IMAGELOADER_IMAGELOADER_H_

#include <string>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/memory/weak_ptr.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/errors/error.h>

#include "dbus_adaptors/org.chromium.ImageLoaderInterface.h"
#include "imageloader_impl.h"

namespace imageloader {

// This is a utility that handles mounting and unmounting of
// verified filesystem images that might include binaries intended
// to be run as read only.
class ImageLoader : public brillo::DBusServiceDaemon,
                    public org::chromium::ImageLoaderInterfaceInterface {
 public:
  // User and group to run imageloader as.
  static const char kImageLoaderGroupName[];
  static const char kImageLoaderUserName[];

  ImageLoader(ImageLoaderConfig config);
  ~ImageLoader();

  // Implementations of the public methods interface.
  // Register a component.
  bool RegisterComponent(brillo::ErrorPtr* err, const std::string& name,
                         const std::string& version,
                         const std::string& component_folder_abs_path,
                         bool* out_success) override;

  // TODO(kerrnel): errors should probably be returned using the err object.
  // Get component version given component name.
  bool GetComponentVersion(brillo::ErrorPtr* err, const std::string& name,
                           std::string* out_version) override;

  // Load and mount a component.
  bool LoadComponent(brillo::ErrorPtr* err, const std::string& name,
                     std::string* out_mount_point);

 protected:
  int OnInit() override;
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;
  void OnShutdown(int* return_code) override;

 private:
  void PostponeShutdown();

  // Daemon will automatically shutdown after this length of idle time.
  static const int kShutdownTimeoutMilliseconds;

  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  ImageLoaderImpl impl_;
  base::CancelableClosure shutdown_callback_;
  org::chromium::ImageLoaderInterfaceAdaptor dbus_adaptor_{this};

  base::WeakPtrFactory<ImageLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageLoader);
};

}  // namespace imageloader

#endif  // IMAGELOADER_IMAGELOADER_H_
