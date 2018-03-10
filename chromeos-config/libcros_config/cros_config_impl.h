// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_IMPL_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_IMPL_H_

#include "chromeos-config/libcros_config/cros_config_interface.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/values.h>

namespace brillo {

// References a node in the configuration
// This allows us to reference a node whether it is device tree or yaml.
class ConfigNode {
 public:
  ConfigNode();
  // Constructor which uses a device-tree offset
  explicit ConfigNode(int offset);

  // Constructor which uses a yaml dict
  explicit ConfigNode(const base::DictionaryValue* dict);

  // @return true if this node reference is valid (points to an actual node)
  bool IsValid() const;

  // @return offset of the device-tree node, or -1 if not valid
  int GetOffset() const;

  // @return yaml dict for a node, or 0 if not valid
  const base::DictionaryValue* GetDict() const;

  // Test equality for two ConfigNode objectcs
  bool operator==(const ConfigNode& other) const;

 private:
  int node_offset_;  // Device-tree node offset
  const base::DictionaryValue* dict_;   // Yaml dictionary for this node
  bool valid_;       // true if we have a valid node reference
};

class CrosConfigImpl {
 public:
  CrosConfigImpl();
  virtual ~CrosConfigImpl();

  // Internal function to obtain a property value and return a list of log
  // messages on failure. Public for tests.
  // @path: Path to locate. Must start with "/".
  // @prop: Property name to look up
  // @val_out: returns the string value found, if any
  // @log_msgs_out: returns a list of error messages if this function fails
  // @return true if found, false if not found
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out,
                 std::vector<std::string>* log_msgs_out);

  // These two methods mirror those in CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out);
  bool GetAbsPath(const std::string& path,
                  const std::string& prop,
                  std::string* val_out);

  // Init for a particular config file
  // This calls InitCommon() with the given file after reading the identity
  // information for this device.
  // @filepath: Path to configuration file to use
  bool InitForConfig(const base::FilePath& filepath);

  // Common init function for both production and test code.
  // @filepath: path to configuration file (e.g. .dtb).
  // @smbios_file: File containing memory to scan (typically this is /dev/mem)
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  bool InitCommon(const base::FilePath& filepath,
                  const base::FilePath& smbios_file,
                  const base::FilePath& vpd_file);

 private:
  // Runs a quick init check and prints an error to stderr if it fails.
  // @return true if OK, false on error.
  bool InitCheck() const;

  // Obtain the full path for the node at a given node.
  // @node: Node to check.
  // @return path to node, or "unknown" if it 256 characters or more (due to
  // limited buffer space). This is much longer than any expected length so
  // should not happen.
  virtual std::string GetFullPath(ConfigNode node) = 0;

  // Obtain node of a given path, relative to the base node.
  // @base_node: base node.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @return node found, or negative value on error.
  virtual ConfigNode GetPathNode(ConfigNode base_node,
                                 const std::string& path) = 0;

 public:
  // Internal function to obtain a property value based on a node
  // This looks up a property for a path, relative to a given base node.
  // @base_node: base node for the search.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @prop: Property name to look up
  // @val_out: returns the string value found, if any
  // @log_msgs_out: returns a list of error messages if this function fails
  // @return true if found, false if not found
  bool GetString(ConfigNode base_node,
                 const std::string& path,
                 const std::string& prop,
                 std::string* val_out,
                 std::vector<std::string>* log_msgs_out);

 protected:
  // Read the configuratoin into our internal structures
  // @filepath: path to configuration file (e.g. .dtb).
  // @return true if OK, false on error (which is logged)
  virtual bool ReadConfigFile(const base::FilePath& filepath) = 0;

  // Read a property from a node
  // @node: Node to read from
  // @name: Property name to reset
  // @value_out: Returns value read from property, if no error
  // @return length of property value, or -ve on error
  virtual int GetProp(const ConfigNode& node,
                      std::string name,
                      std::string* value_out) = 0;

  // Look up a phandle in a node.
  // Looks up a phandle with the given property name in the given node.
  // @node: Node to look in.
  // @prop_name: Name of property to look up.
  // @node_out: Returns the node the phandle points to, if found.
  // @return true if found, false if not.
  virtual bool LookupPhandle(ConfigNode node,
                             const std::string& prop_name,
                             ConfigNode* node_out) = 0;

  // Select the model / submodel to use
  // Looks up the given name and sku_id in the mapping table and sets the
  // model and submodel that will be used for the duration of execution.
  // @find_name: Platform name to search for
  // @find_sku_id: SKU ID to search for
  // @find_customization_id: Customization ID to search for
  // @return true on success, false on failure
  virtual bool SelectModelConfigByIDs(
      const std::string& find_name,
      int find_sku_id,
      const std::string& find_customization_id) = 0;

  ConfigNode model_node_;       // Model's node
  ConfigNode submodel_node_;    // Submodel's node
  std::string model_name_;      // Name of current model
  std::string submodel_name_;   // Name of current submodel
  std::string platform_name_;   // Platform name associated with the SKU map
  ConfigNode whitelabel_node_;  // Whitelabel model

  // We support a special-case 'whitelabel' node which is inside a model. We
  // check this first on any property reads, since it overrides the model
  // itself.
  ConfigNode whitelabel_tag_node_;
  // List of target directories used to obtain absolute paths
  std::map<std::string, std::string> target_dirs_;
  std::vector<std::string> phandle_props_;  // List of phandle properties
  // Default modes to check when we cannot find the requested node or property
  std::vector<ConfigNode> default_nodes_;
  bool inited_ = false;  // true if the class is ready for use (Init*ed)

  DISALLOW_COPY_AND_ASSIGN(CrosConfigImpl);
};

}  // namespace brillo
#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_IMPL_H_
