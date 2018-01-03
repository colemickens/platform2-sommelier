// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_

#include "chromeos-config/libcros_config/cros_config_interface.h"

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
  bool LookupPhandle(int node_offset, const std::string& prop_name,
                     int *offset_out);

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

  // Read the device identity information
  // On x86 platforms the identify generally comes from two bits of information
  // in the SMBIOS system table. These are set up by AP firmware, so in effect
  // AP firmware sets the device identity. We also support using the VPD
  // (customization_id) to dynamically determine device identity. This has
  // been used to support Whitelabels/Zergs in the past, but can be used to
  // support any applicable use case.
  //
  // For ARM systems we could instead look at the device-tree compatible string,
  // which should be enough to identify the device. Again this is set up by AP
  // firmware, but in fact is built as part of the kernel. The note above about
  // VPD applies to ARM as well.
  //
  // For now we just implement x86 identity, requiring access to memory (to
  // read the SMBIOS table) and the VPD device (to read the customization ID).
  //
  // @smbios_file: File containing memory to scan (typically this is /dev/mem)
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  // @name_out: Platform name read from SMBIOS system table
  // @sku_id_out: SKU ID read from SMBIOS system table
  // @customization_id_out: Whitelabel name read from system table
  bool ReadIdentity(const base::FilePath& smbios_file,
                    const base::FilePath& vpd_file, std::string* name_out,
                    int* sku_id_out, std::string* customization_id_out);

  // Read a string from an SMBIOS string table
  // See the System Management BIOS (SMBIOS) Reference Specification for full
  // details.
  // @table: Table to read from
  // @string_id: ID of string to read. The value 1 means to pick the first
  // string from the the table, i.e. strings are numbered from 1, not 0.
  // @return string found, or "" if not found
  std::string GetSmbiosString(const SmbiosTable& table, unsigned int string_id);

  // Read the system table from memory
  // @smbios_file: File containing SMBIOS tables to scan (typically this is
  // /sys/firmware/dmi/tables/DMI)
  // @table_out: Returns a copy of the table found
  // @return true if found, false if not
  bool GetSystemTable(const base::FilePath& smbios_file,
                      SmbiosTable* table_out);

  // SMBIOS Table Types
  // We only use the system table, but for testing purposes we include BIOS.
  enum SmbiosTypes {
    SMBIOS_TYPE_BIOS = 0,
    SMBIOS_TYPE_SYSTEM,
  };

  // Search through some tables looking for particular table type.
  // All data is copied into the output table, so that the caller does not need
  // to reference the memory region to obtain table information.
  // @type: Type to search for
  // @base_ptr: Start of memory region to search
  // @size: Size of region in bytes
  // @table_out: Returns a copy of the table found
  // @return true if found, false if not
  bool FindAndCopyTable(SmbiosTypes type, const char* base_ptr,
                        unsigned int size, SmbiosTable* table_out);

  // Calculates the length of an SMBIOS string table
  // The strings are stored one after another, each with a trailing nul, and
  // with a final additional nul after the last string. An empty table consists
  // of two nul bytes.
  // @ptr: Pointer to start of table (and therefore first string)
  // @return length of table in bytes
  unsigned int StringTableLength(const char* ptr);

  // Write out files containing fake identity information, used for testing.
  // The two files returned mirror the contents of the files that ReadIdentity()
  // uses to ready identity information.
  // @name: Platform name to write (e.g. 'Reef')
  // @sku_id: SKU ID number to write (e.g. 8)
  // @customization_id: Whitelabel name to write
  // @smbios_file_out: Returns the 'dev/mem'-style file write that was written
  // @vpd_file_out: Returns the '/sys/firmware/vpd/ro/customization_id'-style
  //     file that was written
  // @return true if OK, false on error
  bool FakeIdentity(const std::string& name, int sku_id,
                    const std::string& customization_id,
                    base::FilePath* smbios_file_out,
                    base::FilePath* vpd_file_out);

  // Write SMBIOS tables to a file, for testing
  // Tables are read from /dev/mem. This function writes the tables in the same
  // format as they are found in memory.
  // This is suitable for parsing as a normal SMBIOS structure in memory.
  // We only write a few things, in a very simplistic way, to avoid pulling in
  // an entire SMBIOS library.
  // @smbios_file: File to write to
  // @name: Platform name to write (e.g. 'Reef')
  // @sku_id: SKU ID number to write (e.g. 8)
  // @return true if OK, false on error
  bool WriteFakeTables(base::File& smbios_file, const std::string& name,
                       int sku_id);

  // Decode the device identifiers to resolve the model / submodel
  // The decodes the output from 'mosys platform id' into the two fields
  // @output: Output string from mosys
  // @name_out: Returns the platform name (which may be an empty string)
  // @sku_id_out: Returns the SKU ID (which may be -1 if there is none)
  // @customization_id_out: Returns the customization id (may be empty string)
  // @return true on success, false on failure
  bool DecodeIdentifiers(const std::string &output, std::string* name_out,
                         int* sku_id_out, std::string* customization_id_out);

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
  std::vector<int> default_offsets_;  // Device tree offset of default modes
  bool inited_ = false;          // true if the class is ready for use (Init*ed)


  // JSON configuration
  std::unique_ptr<const base::Value> json_config_ = nullptr;
  const base::DictionaryValue* model_dict_ = nullptr;  // Model root

  DISALLOW_COPY_AND_ASSIGN(CrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
