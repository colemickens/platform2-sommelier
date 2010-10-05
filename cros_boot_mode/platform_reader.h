// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines the PlatformReader base class.  It provides a default file reading
// implementation that pulls in the contents of the given platform_file_path.
// This content is passed to a subclass-defined Process() helper which will
// return an int.  This int should map to the anonymous enum in the
// PlatformReader class or to an extension to that enum in the subclass.
#ifndef CROS_BOOT_MODE_PLATFORM_READER_H_
#define CROS_BOOT_MODE_PLATFORM_READER_H_

#include <sys/types.h>
#include <string>

#include "helpers.h"

namespace cros_boot_mode {

class PlatformReader {
 public:
  enum { kUnsupported = -1 };  // All subclasses should extend this enum.
  PlatformReader();
  virtual ~PlatformReader();
  // Provides a default file reader which then calls an override-able
  // function for processing.  Initialize must leave the class in a usable
  // state even on failure.
  virtual void Initialize();

  // To be provided by the implementation

  // The name of the concrete class.
  virtual const char *name() const = 0;
  // c_str() should return the conversion of the given enum to a lowercase
  // char array with no spaces.
  virtual const char *c_str() const = 0;
  // max_size should return the maximum size that will be read.
  virtual size_t max_size() const = 0;
  // default_platform_file_path should return the char array of the
  // path to the file to be processed.
  virtual const char *default_platform_file_path() const = 0;
  // Called from initialize over the contents of the platform file. It
  // should return an int that matches either kUnsupported or an extension
  // of the anonymous enum.  If the file does not exist, cannot be read,
  // or exceeds the max_size(), Process will be called with (buf, 0) arguments.
  virtual int Process(const char *file_contents, size_t length) = 0;

  // Accessors for the private value.
  // These are largely used for internal references, but kept in public
  // to allow for direct unit testing.
  virtual int value() const {
    return value_;
  }
  virtual void set_value(int value) {
    value_ = value;
  }

  virtual const char *platform_file_path() const;
  virtual void set_platform_file_path(const char *path) {
    platform_file_path_ = path;
  }

 private:
   int value_;
   const char *platform_file_path_;
};

}  // namespace cros_boot_mode
#endif  // CROS_BOOT_MODE_PLATFORM_READER_H_
