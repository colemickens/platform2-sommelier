// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_H_

#include <string>

namespace brillo {

class CrosConfigIdentity {
 public:
  CrosConfigIdentity();
  ~CrosConfigIdentity();

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
                    const base::FilePath& vpd_file,
                    std::string* name_out,
                    int* sku_id_out,
                    std::string* customization_id_out);

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
  bool FakeIdentity(const std::string& name,
                    int sku_id,
                    const std::string& customization_id,
                    base::FilePath* smbios_file_out,
                    base::FilePath* vpd_file_out);

 private:
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
  bool FindAndCopyTable(SmbiosTypes type,
                        const char* base_ptr,
                        unsigned int size,
                        SmbiosTable* table_out);

  // Calculates the length of an SMBIOS string table
  // The strings are stored one after another, each with a trailing nul, and
  // with a final additional nul after the last string. An empty table consists
  // of two nul bytes.
  // @ptr: Pointer to start of table (and therefore first string)
  // @return length of table in bytes
  unsigned int StringTableLength(const char* ptr);

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
  bool WriteFakeTables(base::File* smbios_file,
                       const std::string& name,
                       int sku_id);

  DISALLOW_COPY_AND_ASSIGN(CrosConfigIdentity);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_H_
