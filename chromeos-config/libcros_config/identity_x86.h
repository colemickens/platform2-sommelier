// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_X86_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_X86_H_

#include "chromeos-config/libcros_config/identity.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file.h>

namespace brillo {
struct SmbiosTable;

class CrosConfigIdentityX86 : public CrosConfigIdentity {
 public:
  CrosConfigIdentityX86();
  ~CrosConfigIdentityX86();

  // Read the device identity infromation from the SMBIOS system table.
  // This information is set up by AP firmware, so in effect AP firmware
  // sets the device identity.
  //
  // @smbios_file: File containing memory to scan (typically this is /dev/mem)
  bool ReadSmbiosIdentity(const base::FilePath& smbios_file);

  // Write out fake smbios file containing fake identity information.
  // This is only used for testing.
  // @name: Platform name to write (e.g. 'Reef')
  // @sku_id: SKU ID number to write (e.g. 8)
  // @smbios_file_out: Returns the 'dev/mem'-style file write that was written
  // @return true if OK, false on error
  bool FakeSmbiosIdentity(const std::string& name,
                          int sku_id,
                          base::FilePath* smbios_file_out);

  // @return Name value read via ReadSmbiosIdentity
  const std::string& GetName() const { return name_; }

  // @return SKU ID value read via ReadSmbiosIdentity
  int GetSkuId() const { return sku_id_; }

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

  std::string name_;
  int sku_id_;

  DISALLOW_COPY_AND_ASSIGN(CrosConfigIdentityX86);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_X86_H_
