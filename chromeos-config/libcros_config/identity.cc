// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Look up identity information for the current device
// Also provide a way to fake identity for testing.

#include "chromeos-config/libcros_config/cros_config.h"
#include "chromeos-config/libcros_config/identity.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>

namespace brillo {

// Table header
// The specific table struct follows immediately after this
struct SmbiosHeader {
  uint8_t type;    // enum SmbiosTypes
  uint8_t length;  // length of this table in bytes (excluding string table)
  uint16_t handle;
} __attribute__((packed));

// Table type 1 - system information
struct SmbiosTableSystem {
  uint8_t manufacturer;
  uint8_t name;  // Index of string containing platform name
  uint8_t version;
  uint8_t serial_number;
  uint8_t uuid[16];
  uint8_t wakeup_type;
  uint8_t sku_number;  // Index of string containing SKU name "sku%d"
  uint8_t family;
} __attribute__((packed));

// This is our internal format for a decoded table. It includes the header, the
// data specific to this table type, and a table of strings.
struct SmbiosTable {
  SmbiosHeader header;
  union {
    SmbiosTableSystem system;
    uint8_t data[1024];
  } data;
  std::vector<std::string> strings;
};

CrosConfigIdentity::CrosConfigIdentity() {}

CrosConfigIdentity::~CrosConfigIdentity() {}

bool CrosConfigIdentity::WriteFakeTables(base::File* smbios_file,
                                         const std::string& name,
                                         int sku_id) {
  // Write a header for a BIOS table
  // We don't put anything in this. It is just to test that we can skip tables
  // we don't want to parse.
  // This goes right at the start of the file, corresponding to the start of
  // the /sys/firmware/dmi/tables/DMI file.
  SmbiosHeader hdr;
  memset(&hdr, '\0', sizeof(hdr));
  hdr.type = SMBIOS_TYPE_BIOS;
  hdr.length = 100;  // Arbitrary size smaller than the maximum.
  int pos = hdr.length;
  if (smbios_file->Write(0, reinterpret_cast<char*>(&hdr), sizeof(hdr)) < 0) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write header";
    return false;
  }
  // Write an empty string table after the header record.
  if (smbios_file->Write(pos, "\0\0", 2) < 0) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write empty string table";
    return false;
  }

  // Our system table follows immediately after the BIOS table. It has two
  // indexed strings, numbered 1 and 2.
  SmbiosTableSystem system;
  memset(&system, '\0', sizeof(system));
  system.name = 1;
  system.sku_number = 2;

  std::string sku_id_str = base::StringPrintf("sku%d", sku_id);
  hdr.type = SMBIOS_TYPE_SYSTEM;
  hdr.length = sizeof(hdr) + sizeof(system);
  if (smbios_file->Seek(base::File::FROM_BEGIN, pos + 2) == -1) {
    CROS_CONFIG_LOG(ERROR) << "Failed to seek";
  }

  // This is the system table. First write the header and the table data.
  if (smbios_file->WriteAtCurrentPos(reinterpret_cast<char*>(&hdr),
                                     sizeof(hdr)) < 0) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write system header";
    return false;
  }
  if (smbios_file->WriteAtCurrentPos(reinterpret_cast<char*>(&system),
                                     sizeof(system)) < 0) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write system table";
    return false;
  }

  // Next write the string table and terminator.
  if (smbios_file->WriteAtCurrentPos(name.c_str(), name.length() + 1) < 0) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write name";
    return false;
  }
  if (smbios_file->WriteAtCurrentPos(sku_id_str.c_str(),
                                     sku_id_str.length() + 1) < 0) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write SKU string";
    return false;
  }
  char zero = 0;
  if (smbios_file->WriteAtCurrentPos(&zero, 1) < 0) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write terminator";
    return false;
  }
  return true;
}

bool CrosConfigIdentity::FakeIdentity(const std::string& name, int sku_id,
                                      const std::string& customization_id,
                                      base::FilePath* smbios_file_out,
                                      base::FilePath* vpd_file_out) {
  *vpd_file_out = base::FilePath("vpd");
  if (base::WriteFile(*vpd_file_out, customization_id.c_str(),
                      customization_id.length()) != customization_id.length()) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write VPD file";
    return false;
  }

  // Write out to a temporary file used just for testing.
  *smbios_file_out = base::FilePath("dev_mem");
  base::File dev_mem(*smbios_file_out,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!dev_mem.IsValid()) {
    CROS_CONFIG_LOG(ERROR) << "Failed to create dev_mem";
    return false;
  }
  if (!WriteFakeTables(&dev_mem, name, sku_id)) {
    CROS_CONFIG_LOG(ERROR) << "Failed to write tables";
    return false;
  }

  return true;
}

unsigned int CrosConfigIdentity::StringTableLength(const char* ptr) {
  // Use a special case for an empty string table.
  if (!*ptr)
    return 2;

  int len, total_len;
  for (total_len = 0; *ptr; ptr += len) {
    len = strlen(ptr) + 1;
    total_len += len;
  }
  return total_len + 1;
}

bool CrosConfigIdentity::FindAndCopyTable(enum SmbiosTypes type,
                                          const char* base_ptr,
                                          unsigned int size,
                                          SmbiosTable* table_out) {
  // Look through the region for the table type we want.
  const SmbiosHeader* hdr = 0;
  const char* ptr;
  for (ptr = base_ptr; ptr < base_ptr + size;
       ptr += hdr->length, ptr += StringTableLength(ptr)) {
    hdr = reinterpret_cast<const SmbiosHeader*>(ptr);
    if (hdr->type == type) {
      break;
    }
  }
  if (!hdr || hdr->type != type) {
    CROS_CONFIG_LOG(ERROR) << "Table type " << type
                           << " not found in system table";
    return false;
  }
  // Copy out the table header and contents
  memcpy(&table_out->header, hdr, sizeof(*hdr));
  memcpy(table_out->data.data, hdr + 1, hdr->length - sizeof(*hdr));

  // Figure out the size of the string table, then copy that too.
  const char* strings_ptr = ptr + hdr->length;
  while (*strings_ptr) {
    const int len = strlen(strings_ptr);
    table_out->strings.emplace_back(strings_ptr, len);
    strings_ptr += len + 1;
  }
  VLOG(1) << "found SMBIOS table";

  return true;
}

bool CrosConfigIdentity::GetSystemTable(const base::FilePath& smbios_file,
                                        SmbiosTable* table_out) {
  int fd = open(smbios_file.MaybeAsASCII().c_str(), O_RDONLY);
  if (fd < 0) {
    CROS_CONFIG_LOG(ERROR) << "Could not open " << smbios_file.MaybeAsASCII()
                           << ": " << strerror(errno);
    return false;
  }
  struct stat buf;
  if (fstat(fd, &buf) == -1) {
    CROS_CONFIG_LOG(ERROR) << "Could not get file size of"
                           << smbios_file.MaybeAsASCII() << ": "
                           << strerror(errno);
  }
  unsigned int table_size = buf.st_size;
  void* ptr = mmap(0, table_size, PROT_READ, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    close(fd);
    CROS_CONFIG_LOG(ERROR) << "Could not map file "
                           << smbios_file.MaybeAsASCII() << ": "
                           << strerror(errno);
    return false;
  }

  // Find the system table and copy it out.
  bool ok = FindAndCopyTable(SMBIOS_TYPE_SYSTEM, static_cast<char*>(ptr),
                             table_size, table_out);
  munmap(ptr, table_size);
  close(fd);
  return ok;
}

std::string CrosConfigIdentity::GetSmbiosString(const SmbiosTable& table,
                                                unsigned int string_id) {
  // The first string is numbered 1, so we need to subtract one first.
  string_id--;
  if (string_id < table.strings.size()) {
    return table.strings[string_id];
  }

  return std::string();
}

bool CrosConfigIdentity::ReadIdentity(const base::FilePath& smbios_file,
                                      const base::FilePath& vpd_file,
                                      std::string* name_out, int* sku_id_out,
                                      std::string* customization_id_out) {
  SmbiosTable table;
  if (!GetSystemTable(smbios_file, &table)) {
    CROS_CONFIG_LOG(ERROR) << "Could not get system table";
    return false;
  }

  // Get the system name and decode the SKU ID from its string.
  *name_out = GetSmbiosString(table, table.data.system.name);
  std::string sku_str = GetSmbiosString(table, table.data.system.sku_number);
  if (std::sscanf(sku_str.c_str(), "sku%d", sku_id_out) != 1) {
    CROS_CONFIG_LOG(WARNING) << "Invalid SKU string: " << sku_str;
    *sku_id_out = -1;
  }
  if (!base::ReadFileToString(vpd_file, customization_id_out)) {
    CROS_CONFIG_LOG(WARNING) << "No customization_id in VPD";
    // This file is only used for whitelabels, so may be missing. Without it
    // we rely on just the name and SKU ID.
  }
  CROS_CONFIG_LOG(INFO) << "name: " << *name_out << ", sku_id: " << *sku_id_out
                        << ", customization_id: " << *customization_id_out;
  return true;
}

}  // namespace brillo
