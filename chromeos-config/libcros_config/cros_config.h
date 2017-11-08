// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_

#include "chromeos-config/libcros_config/cros_config_interface.h"

#include <string>
#include <vector>

#include <base/macros.h>
#include <brillo/brillo_export.h>

namespace base {
class CommandLine;
class FilePath;
}

namespace brillo {

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
  // @whitelabel_name: Name of the whitelabel model or 'tag' node
  // @return true if OK, false on error.
  bool InitForTest(const base::FilePath& filepath, const std::string& name,
                   int sku_id, const std::string& whitelabel_tag);

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out) override;

  // CrosConfigInterface:
  bool GetAbsPath(const std::string& path, const std::string& prop,
                  std::string* val_out) override;

 private:
  // Common init function for both production and test code.
  // @filepath: path to configuration .dtb file.
  // @cmdline: command line to execute to find out the current model. This is
  // normally something that runs the 'mosys' tool.
  bool InitCommon(const base::FilePath& filepath,
                  const base::CommandLine& cmdline);

  // Runs a quick init check and prints an error to stderr if it fails.
  // @return true if OK, false on error.
  bool InitCheck() const;

  // Obtain the full path for the node at a given offset.
  // @offset: offset of the node to check.
  // @return path to node, or "unknown" if it 256 characters or more (due to
  // limited buffer space). This is much longer than any expected length so
  // should not happen.
  std::string GetFullPath(int offset);

  // Obtain offset of a given path, relative to the base node.
  // @base_offset: offset of base node.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @return node offset of the node found, or negative value on error.
  int GetPathOffset(int base_offset, const std::string& path);

  // Internal function to obtain a property value based on a node offset
  // This looks up a property for a path, relative to a given base node offset.
  // @base_offset: offset of base node for the search.
  // @path: Path to locate (relative to @base). Must start with "/".
  // @prop: Property name to look up
  // @val_out: returns the string value found, if any
  // @return true if found, false if not found
  bool GetString(int base_offset, const std::string& path,
                 const std::string& prop, std::string* val_out);

  // Look up a phandle in a node.
  // Looks up a phandle with the given property name in the given node.
  // @node_offset: Offset of node to look in.
  // @prop_name: Name of property to look up.
  // @offset_out: Returns the offset of the node the phandle points to, if
  // found.
  // @return true if found, false if not.
  bool LookupPhandle(int node_offset, const std::string &prop_name,
                     int *offset_out);

  // Select the model / submodel to use
  // Looks up the given name and sku_id in the mapping table and sets the
  // model and submodel that will be used for the duration of execution.
  // @find_name: Platform name to search for
  // @find_sku_id: SKU ID to search for
  // @find_whitelabel_name: Whitelabel model or tag to search for
  // @return true on success, false on failure
  bool SelectModelConfigByIDs(const std::string &find_name, int find_sku_id,
                              std::string& find_whitelabel_name);

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

  // Decode the device identifiers to resolve the model / submodel
  // The decodes the output from 'mosys platform id' into the two fields
  // @output: Output string from mosys
  // @name_out: Returns the platform name (which may be an exmpty string)
  // @sku_id: Returns the SKU ID (which may be -1 if there is none)
  // @return true on success, false on failure
  bool DecodeIdentifiers(const std::string &output, std::string* name_out,
                         int* sku_id_out, std::string* whitelabel_name_out);

  std::string blob_;             // Device tree binary blob
  std::string model_;            // Model name for this device
  int model_offset_ = -1;        // Device tree offset of the model's node
  int submodel_offset_ = -1;     // Device tree offset of the submodel's node
  std::string model_name_;       // Name of current model
  std::string submodel_name_;    // Name of current submodel
  std::string platform_name_;    // Platform name associated with the SKU map
  int whitelabel_offset_ = -1;   // Device tree offset of the whitelabel model

  // We support a special-case 'whitelabel' node which is inside a model. This
  // holds the device tree offset of that node. We check this first on any
  // property reads, since it overrides the model itself.
  int whitelabel_tag_offset_ = -1;
  int target_dirs_offset_ = -1;  // Device tree offset of the target-dirs node
  int default_offset_ = -1;      // Device tree offset of the default mode
  bool inited_ = false;          // true if the class is ready for use (Init*ed)
  DISALLOW_COPY_AND_ASSIGN(CrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
