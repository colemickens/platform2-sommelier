// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGELOADER_IMAGELOADCLIENT_H_
#define IMAGELOADER_IMAGELOADCLIENT_H_

#include <string>

#include <dbus-c++/dbus.h>

#include "imageloadclient-glue.h"

namespace imageloader {

// This is a simple client to use imageloader's service in non-root mode.
class ImageLoadClient
    : public org::chromium::ImageLoaderInterface_proxy,
      public DBus::ObjectProxy {
 public:
  // Initialize the ImageLoadClient instance.
  ImageLoadClient(DBus::Connection* conn, const char* path, const char* name);

  void RegisterComponentCallback(const bool& success, const ::DBus::Error& err,
                                 void* data);

  void GetComponentVersionCallback(const std::string& version,
                                   const ::DBus::Error& err, void* data);

  void LoadComponentCallback(const std::string& mount_point,
                             const ::DBus::Error& err, void* data);

  void UnloadComponentCallback(const bool& success, const ::DBus::Error& err,
                               void* data);
};

}  // namespace imageloader

#endif  // IMAGELOADER_IMAGELOADCLIENT_H_
