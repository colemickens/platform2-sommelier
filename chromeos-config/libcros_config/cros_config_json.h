// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// YAML implementation of master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_YAML_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_YAML_H_

#include "chromeos-config/libcros_config/cros_config_impl.h"

#include <memory>
#include <string>

namespace brillo {

class CrosConfigJson : public CrosConfigImpl {
 public:
  CrosConfigJson();
  virtual ~CrosConfigJson();

 protected:
  // CrosConfigImpl:
  std::string GetFullPath(ConfigNode node) override;

  // CrosConfigImpl:
  ConfigNode GetPathNode(ConfigNode base_node,
                         const std::string& path) override;

  // CrosConfigImpl:
  int GetProp(const ConfigNode& node,
              std::string name,
              std::string* value_out) override;

  // CrosConfigImpl:
  bool LookupPhandle(ConfigNode node,
                     const std::string& prop_name,
                     ConfigNode* node_out) override;

  // CrosConfigImpl:
  bool SelectModelConfigByIDs(
      const std::string& find_name,
      int find_sku_id,
      const std::string& find_customization_id) override;

 private:
  // CrosConfigImpl:
  bool ReadConfigFile(const base::FilePath& filepath) override;

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

  std::unique_ptr<const base::Value> json_config_ = nullptr;
  const base::DictionaryValue* model_dict_ = nullptr;  // Model root
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_YAML_H_
