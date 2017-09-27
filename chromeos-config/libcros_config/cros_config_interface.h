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

  // Obtain a list of all firmware URIs attached to the inited model. Just like
  // GetString, InitModel or InitHost must be called with a valid model name
  // before calling this.
  // @return A space separated list of firmware URIs.
  virtual std::vector<std::string> GetFirmwareUris() const = 0;

  // Obtain a config property.
  // This returns a property for the current board model. This can only be
  // called if Init*() was called, and either InitModel was called OR InitHost
  // was called with a valid model name.
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

  // Obtain a list of all model names
  // This will return a list of all model names in a config, or an empty vector.
  // This much be called after Init*().
  // @return A list of all models in a config
  virtual std::vector<std::string> GetModelNames() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosConfigInterface);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_INTERFACE_H_
