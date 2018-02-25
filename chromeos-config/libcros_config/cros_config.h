// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_

#include "chromeos-config/libcros_config/cros_config_interface.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/values.h>
#include <brillo/brillo_export.h>

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace brillo {

struct SmbiosTable;

// References a node in the configuration
// This allows us to reference a node whether it is device tree or yaml.
class ConfigNode {
 public:
  ConfigNode();
  // Constructor which uses a device-tree offset
  explicit ConfigNode(int offset);

  // @return true if this node reference is valid (points to an actual node)
  bool IsValid() const;

  // @return offset of the device-tree node, or -1 if not valid
  int GetOffset() const;

  // Test equality for two ConfigNode objectcs
  bool operator==(const ConfigNode& other) const;

 private:
  int node_offset_;         // Device-tree node offset
  bool valid_;              // true if we have a valid node reference
};


class BRILLO_EXPORT CrosConfig : public CrosConfigInterface {
 public:
  CrosConfig();
  ~CrosConfig() override;

  // Prepare the configuration system for for access to the configuration for
  // the model this is running on. This reads the configuration file into
  // memory.
  // @return true if OK, false on error.
  bool InitModel();

  // Alias for the above, since this is used by several clients.
  bool Init();

  // Prepare the configuration system for testing.
  // This reads in the given configuration file and selects the supplied
  // model name.
  // @filepath: Path to configuration .dtb file.
  // @name: Platform name as returned by 'mosys platform id'.
  // @sku_id: SKU ID as returned by 'mosys platform sku'.
  // @customization_id: VPD customization ID from 'mosys platform customization'
  // @return true if OK, false on error.
  bool InitForTest(const base::FilePath& filepath, const std::string& name,
                   int sku_id, const std::string& customization_id);

  // Internal function to obtain a property value and return a list of log
  // messages on failure. Public for tests.
  // @path: Path to locate. Must start with "/".
  // @prop: Property name to look up
  // @val_out: returns the string value found, if any
  // @log_msgs_out: returns a list of error messages if this function fails
  // @return true if found, false if not found
  bool GetString(const std::string& path, const std::string& prop,
                 std::string* val_out, std::vector<std::string>* log_msgs_out);

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out) override;

  // CrosConfigInterface:
  bool GetAbsPath(const std::string& path, const std::string& prop,
                  std::string* val_out) override;

 private:
  // Init for a particular config file
  // This calls InitCommon() with the given file after reading the identity
  // information for this device.
  // @filepath: Path to configuration file to use
  bool InitForConfig(const base::FilePath& filepath);

  // Common init function for both production and test code.
  // @filepath: path to configuration .dtb file.
  // @smbios_file: File containing memory to scan (typically this is /dev/mem)
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  bool InitCommon(const base::FilePath& filepath,
                  const base::FilePath& smbios_file,
                  const base::FilePath& vpd_file);

  // Runs a quick init check and prints an error to stderr if it fails.
  // @return true if OK, false on error.
  bool InitCheck() const;

  // Obtain the full path for the node at a given node.
  // @node: Node to check.
  // @return path to node, or "unknown" if it 256 characters or more (due to
  // limited buffer space). This is much longer than any expected length so
  // should not happen.
  std::string GetFullPath(const ConfigNode& node);

  // Obtain node of a given path, relative to the base node.
  // @base_node: base node.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @return node found, or negative value on error.
  ConfigNode GetPathNode(const ConfigNode& base_node, const std::string& path);

  // Internal function to obtain a property value based on a node
  // This looks up a property for a path, relative to a given base node.
  // @base_node: base node for the search.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @prop: Property name to look up
  // @val_out: returns the string value found, if any
  // @log_msgs_out: returns a list of error messages if this function fails
  // @return true if found, false if not found
  bool GetString(const ConfigNode& base_node, const std::string& path,
                 const std::string& prop, std::string* val_out,
                 std::vector<std::string>* log_msgs_out);

  // Look up a phandle in a node.
  // Looks up a phandle with the given property name in the given node.
  // @node: Node to look in.
  // @prop_name: Name of property to look up.
  // @node_out: Returns the node the phandle points to, if found.
  // @return true if found, false if not.
  bool LookupPhandle(const ConfigNode& node,
                     const std::string& prop_name,
                     ConfigNode* node_out);

  // Select the model / submodel to use
  // Looks up the given name and sku_id in the mapping table and sets the
  // model and submodel that will be used for the duration of execution.
  // @find_name: Platform name to search for
  // @find_sku_id: SKU ID to search for
  // @find_customization_id: Customization ID to search for
  // @return true on success, false on failure
  bool SelectModelConfigByIDs(const std::string& find_name, int find_sku_id,
                              const std::string& find_customization_id);

  // Check a single sku-map node for a match
  // This searches the given sku-map node to see if it is a match for the given
  // SKU ID.
  // @node: 'sku-map' node to examine
  // @find_sku_id: SKU ID to search for. This is not required (can be -1) for
  // single-sku matching
  // @find_name: Platform name to search for. Can be empty if the name does
  // not need to be checked (no smbios-name-match property). This only works
  // on x86 devices at present although it should be easily extensible to ARM.
  // @platform_name_out: Returns platform name for this SKU, if found
  // @return the phandle to a model or submodel node that was found (> 0), or 0
  // if not found, or negative on error
  int FindIDsInMap(int node, const std::string& find_name, int find_sku_id,
                   std::string* platform_name_out);

  // Check all sku-map nodes for a match
  // This searches all the sku-map subnodes to see if one is a match for the
  // given SKU ID.
  // @mapping_node: 'mapping' node to examine
  // @find_name: platform name to search for
  // @find_sku_id: SKU ID to search for
  // @platform_name_out: Returns platform name for this SKU, if found
  // @return the phandle to a model or submodel node that was found (> 0), or 0
  // if not found, or negative on error
  int FindIDsInAllMaps(int mapping_node, const std::string& find_name,
                       int find_sku_id, std::string* platform_name_out);

  // Find the model node pointed to by a phandle
  // Note that a SKU map can point to either a model node or a submodel node.
  // In the latter case, this function still returns the model node, but the
  // submodel node is available in @target_out.
  // @phandle: Phandle to look up
  // @target_out: Returns the target node of the phandle, which may be a model
  // node or a submodel node
  // @return model node for this phandle, or negative on error
  int FollowPhandle(int phandle, int* target_out);

  std::string blob_;             // Device tree binary blob
  std::string model_;            // Model name for this device
  ConfigNode model_node_;        // Model's node
  ConfigNode submodel_node_;     // Submodel's node
  std::string model_name_;       // Name of current model
  std::string submodel_name_;    // Name of current submodel
  std::string platform_name_;    // Platform name associated with the SKU map
  ConfigNode whitelabel_node_;   // Whitelabel model

  // We support a special-case 'whitelabel' node which is inside a model. We
  // check this first on any property reads, since it overrides the model
  // itself.
  ConfigNode whitelabel_tag_node_;
  // List of target directories used to obtain absolute paths
  std::map<std::string, std::string> target_dirs_;
  std::vector<std::string> phandle_props_;  // List of phandle properties
  // Default modes to check when we cannot find the requested node or property
  std::vector<ConfigNode> default_nodes_;
  bool inited_ = false;          // true if the class is ready for use (Init*ed)


  // JSON configuration
  std::unique_ptr<const base::Value> json_config_ = nullptr;
  const base::DictionaryValue* model_dict_ = nullptr;  // Model root

  DISALLOW_COPY_AND_ASSIGN(CrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
