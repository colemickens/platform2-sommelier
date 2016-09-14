// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IMAGELOADER_IMAGELOADER_H_
#define IMAGELOADER_IMAGELOADER_H_

#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "imageloader_common.h"
#include "imageloader-glue.h"
#include "imageloader_impl.h"

namespace imageloader {

// This is a utility that handles mounting and unmounting of
// verified filesystem images that might include binaries intended
// to be run as read only.
class ImageLoader : org::chromium::ImageLoaderInterface_adaptor,
                    public DBus::ObjectAdaptor {
 public:
  // Instantiate a D-Bus Helper Instance
  ImageLoader(DBus::Connection* conn, ImageLoaderConfig config)
      : DBus::ObjectAdaptor(*conn, kImageLoaderPath),
        impl_(std::move(config)) {}

  // Register a component.
  bool RegisterComponent(const std::string& name, const std::string& version,
                         const std::string& component_folder_abs_path,
                         ::DBus::Error& err) {
    return impl_.RegisterComponent(name, version, component_folder_abs_path);
  }

  // TODO(kerrnel): errors should probably be returned using the err object.
  // Get component version given component name.
  std::string GetComponentVersion(const std::string& name, ::DBus::Error& err) {
    return impl_.GetComponentVersion(name);
  }

 private:
  ImageLoaderImpl impl_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoader);
};

}  // namespace imageloader

#endif  // IMAGELOADER_IMAGELOADER_H_
