// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_DBUS_FILE_DESCRIPTOR_H_
#define LIBBRILLO_BRILLO_DBUS_FILE_DESCRIPTOR_H_

namespace brillo {
namespace dbus_utils {

// This struct wraps file descriptors to give them a type other than int.
// Implicit conversions are provided because this should be as transparent
// a wrapper as possible to match the libchrome bindings below when this
// class is used by chromeos-dbus-bindings.
struct FileDescriptor {
  FileDescriptor(int fd) : fd(fd) {}

  inline FileDescriptor& operator=(int new_fd) {
    fd = new_fd;
    return *this;
  }

  int fd;
};

}  // namespace dbus_utils
}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_DBUS_FILE_DESCRIPTOR_H_
