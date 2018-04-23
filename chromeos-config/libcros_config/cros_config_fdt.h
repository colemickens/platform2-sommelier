// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FDT_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FDT_H_

#include "chromeos-config/libcros_config/cros_config_impl.h"

#include <string>
#include <vector>

namespace brillo {

// References a node in the configuration.
// This allows us to reference a node whether it is device tree or JSON.
class ConfigNode {
 public:
  ConfigNode();
  // Constructor which uses a device-tree offset
  explicit ConfigNode(int offset);

  // @return true if this node reference is valid (points to an actual node)
  bool IsValid() const { return valid_; }

  // @return offset of the device-tree node, or -1 if not valid
  int GetOffset() const;

  // Test equality for two ConfigNode objectcs
  bool operator==(const ConfigNode& other) const;

 private:
  bool valid_;       // true if we have a valid node reference
  int node_offset_;  // Device-tree node offset
};

class CrosConfigFdt : public CrosConfigImpl {
 public:
  CrosConfigFdt();
  ~CrosConfigFdt() override;

  // CrosConfig:
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out,
                 std::vector<std::string>* log_msgs_out) override;

  // CrosConfigImpl:
  bool ReadConfigFile(const base::FilePath& filepath) override;
  bool SelectConfigByIdentityX86(
      const CrosConfigIdentityX86& identity) override;

 private:
  // Internal function to obtain a property value based on a node
  // This looks up a property for a path, relative to a given base node.
  // @base_node: base node for the search.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @prop: Property name to look up
  // @val_out: returns the string value found, if any
  // @log_msgs_out: returns a list of error messages if this function fails
  // @return true if found, false if not found
  bool GetStringByNode(const ConfigNode& base_node,
                       const std::string& path,
                       const std::string& prop,
                       std::string* val_out,
                       std::vector<std::string>* log_msgs_out);

  // Read a property from a node
  // @node: Node to read from
  // @name: Property name to reset
  // @value_out: Returns value read from property, if no error
  // @return length of property value, or -ve on error
  int GetProp(const ConfigNode& node, std::string name, std::string* value_out);

  // Look up a phandle in a node.
  // Looks up a phandle with the given property name in the given node.
  // @node: Node to look in.
  // @prop_name: Name of property to look up.
  // @node_out: Returns the node the phandle points to, if found.
  // @return true if found, false if not.
  bool LookupPhandle(const ConfigNode& node,
                     const std::string& prop_name,
                     ConfigNode* node_out);

  // Obtain the full path for the node at a given node.
  // @node: Node to check.
  // @return path to node, or "unknown" if it's 256 characters or more (due to
  // limited buffer space). This is much longer than any expected length so
  // should not happen.
  std::string GetFullPath(const ConfigNode& node);

  // Obtain node of a given path, relative to the base node.
  // @base_node: base node.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @return node found, or negative value on error.
  ConfigNode GetPathNode(const ConfigNode& base_node, const std::string& path);

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
  int FindIDsInMap(int node,
                   const std::string& find_name,
                   int find_sku_id,
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
  int FindIDsInAllMaps(int mapping_node,
                       const std::string& find_name,
                       int find_sku_id,
                       std::string* platform_name_out);

  // Find the model node pointed to by a phandle
  // Note that a SKU map can point to either a model node or a submodel node.
  // In the latter case, this function still returns the model node, but the
  // submodel node is available in @target_out.
  // @phandle: Phandle to look up
  // @target_out: Returns the target node of the phandle, which may be a model
  // node or a submodel node
  // @return model node for this phandle, or negative on error
  int FollowPhandle(int phandle, int* target_out);

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
  std::vector<std::string> phandle_props_;  // List of phandle properties
  // Default modes to check when we cannot find the requested node or property
  std::vector<ConfigNode> default_nodes_;

  std::string blob_;  // Device tree binary blob

  DISALLOW_COPY_AND_ASSIGN(CrosConfigFdt);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FDT_H_
