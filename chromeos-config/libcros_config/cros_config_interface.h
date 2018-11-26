// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_INTERFACE_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_INTERFACE_H_

#include <string>
#include <vector>

#include <base/macros.h>

namespace brillo {

// Interface definition for accessing the Chrome OS master configuration.
class CrosConfigInterface {
 public:
  CrosConfigInterface() {}
  virtual ~CrosConfigInterface() {}

  // Obtain a config property.
  // This returns a property for the current board model. This can only be
  // called if Init*() was called, and either InitModel was called was called
  // with a valid model name.
  // @path: Path to property ("/" for a property at the top of the model
  // hierarchy). The path specifies the node that contains the property to be
  // accessed.
  // @prop: Name of property to look up. This is separate from the path since
  // nodes and properties are separate concepts in device tree, and mixing
  // nodes and properties in paths is frowned upon. Also it is typical when
  // reading properties to access them all from a single node, so having the
  // path the same in each case allows a constant to be used for @path.
  // @val_out: returns the string value found, if any
  // @return true on success, false on failure (e.g. no such property)
  virtual bool GetString(const std::string& path,
                         const std::string& prop,
                         std::string* val_out) = 0;

  // Return true iff library debug logging is enabled.
  // Currently this checks for a non-empty CROS_CONFIG_DEBUG environment
  // variable.
  static bool IsLoggingEnabled();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosConfigInterface);
};

#define CROS_CONFIG_LOG(severity) \
  LOG_IF(severity, CrosConfigInterface::IsLoggingEnabled())

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_INTERFACE_H_
