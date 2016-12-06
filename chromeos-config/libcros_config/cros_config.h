// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_

#include <string>

#include <base/macros.h>

namespace base {
class CommandLine;
class FilePath;
}

namespace brillo {

class CrosConfig {
 public:
  CrosConfig();
  ~CrosConfig();

  // Prepare the configuration system for use.
  // This reads the configuration file into memory.
  // @return true if OK, false on error.
  bool Init();

  // Prepare the configuration system for testing.
  // This reads in the given configuration file and selects the supplied
  // model name.
  // @filepath: Patch to configuration .dtb file.
  // @model: Model name (e.g. 'reef').
  // @return true if OK, false on error.
  bool InitForTest(const base::FilePath& filepath, const std::string& model);

  // Obtain a config property.
  // This returns a property for the current board model.
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
  bool GetString(const std::string& path, const std::string& prop,
                 std::string* val_out) const;

 private:
  // Common init function for both production and test code.
  // @filepath: path to configuration .dtb file.
  // @cmdline: command line to execute to find out the current model. This is
  // normally something that runs the 'mosys' tool.
  bool InitCommon(const base::FilePath& filepath,
                  const base::CommandLine& cmdline);

  std::string blob_;       // Device tree binary blob
  std::string model_;      // Model name for this device
  int model_offset_ = -1;  // Device tree offset of the model's node
  bool inited_ = false;    // true if the class is ready for use (Init() called)
  DISALLOW_COPY_AND_ASSIGN(CrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
