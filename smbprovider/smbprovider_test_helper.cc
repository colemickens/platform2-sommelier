// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/smbprovider_test_helper.h"

#include <algorithm>
#include <utility>

#include <gtest/gtest.h>

#include "smbprovider/mount_config.h"
#include "smbprovider/netbios_packet_parser.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/temp_file_manager.h"

namespace smbprovider {
namespace {

template <typename ProtoType>
ProtoBlob SerializeProtoToBlobAndCheck(const ProtoType& proto) {
  ProtoBlob proto_blob;
  EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(proto, &proto_blob));
  return proto_blob;
}

}  // namespace

MountOptionsProto CreateMountOptionsProto(const std::string& path,
                                          const std::string& workgroup,
                                          const std::string& username) {
  MountOptionsProto mount_options;
  mount_options.set_path(path);
  mount_options.set_workgroup(workgroup);
  mount_options.set_username(username);

  // Default to enable NTLM authentication.
  std::unique_ptr<MountConfigProto> config =
      CreateMountConfigProto(true /* enable_ntlm */);

  mount_options.set_allocated_mount_config(config.release());

  return mount_options;
}

MountOptionsProto CreateMountOptionsProto(const std::string& path,
                                          const std::string& workgroup,
                                          const std::string& username,
                                          const MountConfig& mount_config) {
  MountOptionsProto mount_options =
      CreateMountOptionsProto(path, workgroup, username);

  std::unique_ptr<MountConfigProto> config =
      CreateMountConfigProto(mount_config.enable_ntlm);

  mount_options.set_allocated_mount_config(config.release());

  return mount_options;
}

UnmountOptionsProto CreateUnmountOptionsProto(int32_t mount_id) {
  UnmountOptionsProto unmount_options;
  unmount_options.set_mount_id(mount_id);
  return unmount_options;
}

ReadDirectoryOptionsProto CreateReadDirectoryOptionsProto(
    int32_t mount_id, const std::string& directory_path) {
  ReadDirectoryOptionsProto read_directory_options;
  read_directory_options.set_mount_id(mount_id);
  read_directory_options.set_directory_path(directory_path);
  return read_directory_options;
}

GetMetadataEntryOptionsProto CreateGetMetadataOptionsProto(
    int32_t mount_id, const std::string& entry_path) {
  GetMetadataEntryOptionsProto get_metadata_options;
  get_metadata_options.set_mount_id(mount_id);
  get_metadata_options.set_entry_path(entry_path);
  return get_metadata_options;
}

OpenFileOptionsProto CreateOpenFileOptionsProto(int32_t mount_id,
                                                const std::string& file_path,
                                                bool writeable) {
  OpenFileOptionsProto open_file_options;
  open_file_options.set_mount_id(mount_id);
  open_file_options.set_file_path(file_path);
  open_file_options.set_writeable(writeable);
  return open_file_options;
}

CloseFileOptionsProto CreateCloseFileOptionsProto(int32_t mount_id,
                                                  int32_t file_id) {
  CloseFileOptionsProto close_file_options;
  close_file_options.set_mount_id(mount_id);
  close_file_options.set_file_id(file_id);
  return close_file_options;
}

DeleteEntryOptionsProto CreateDeleteEntryOptionsProto(
    int32_t mount_id, const std::string& entry_path, bool recursive) {
  DeleteEntryOptionsProto delete_entry_options;
  delete_entry_options.set_mount_id(mount_id);
  delete_entry_options.set_entry_path(entry_path);
  delete_entry_options.set_recursive(recursive);
  return delete_entry_options;
}

ReadFileOptionsProto CreateReadFileOptionsProto(int32_t mount_id,
                                                int32_t file_id,
                                                int64_t offset,
                                                int32_t length) {
  ReadFileOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_file_id(file_id);
  options.set_offset(offset);
  options.set_length(length);
  return options;
}

CreateFileOptionsProto CreateCreateFileOptionsProto(
    int32_t mount_id, const std::string& file_path) {
  CreateFileOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_file_path(file_path);
  return options;
}

TruncateOptionsProto CreateTruncateOptionsProto(int32_t mount_id,
                                                const std::string& file_path,
                                                int64_t length) {
  TruncateOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_file_path(file_path);
  options.set_length(length);
  return options;
}

WriteFileOptionsProto CreateWriteFileOptionsProto(int32_t mount_id,
                                                  int32_t file_id,
                                                  int64_t offset,
                                                  int32_t length) {
  WriteFileOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_file_id(file_id);
  options.set_offset(offset);
  options.set_length(length);
  return options;
}

CreateDirectoryOptionsProto CreateCreateDirectoryOptionsProto(
    int32_t mount_id, const std::string& directory_path, bool recursive) {
  CreateDirectoryOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_directory_path(directory_path);
  options.set_recursive(recursive);
  return options;
}

MoveEntryOptionsProto CreateMoveEntryOptionsProto(
    int32_t mount_id,
    const std::string& source_path,
    const std::string& target_path) {
  MoveEntryOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_source_path(source_path);
  options.set_target_path(target_path);
  return options;
}

CopyEntryOptionsProto CreateCopyEntryOptionsProto(
    int32_t mount_id,
    const std::string& source_path,
    const std::string& target_path) {
  CopyEntryOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_source_path(source_path);
  options.set_target_path(target_path);
  return options;
}

GetDeleteListOptionsProto CreateGetDeleteListOptionsProto(
    int32_t mount_id, const std::string& entry_path) {
  GetDeleteListOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_entry_path(entry_path);
  return options;
}

GetSharesOptionsProto CreateGetSharesOptionsProto(
    const std::string& server_url) {
  GetSharesOptionsProto options;
  options.set_server_url(server_url);
  return options;
}

RemountOptionsProto CreateRemountOptionsProto(const std::string& path,
                                              const std::string& workgroup,
                                              const std::string& username,
                                              int32_t mount_id,
                                              MountConfig mount_config) {
  RemountOptionsProto options;
  options.set_path(path);
  options.set_mount_id(mount_id);
  options.set_workgroup(workgroup);
  options.set_username(username);

  // set_allocated_mount_config() transfers ownership of |mount_config| to
  // |mount_options| so |mount_config| needs to be created in the heap.
  auto config = std::make_unique<MountConfigProto>();
  config->set_enable_ntlm(mount_config.enable_ntlm);

  options.set_allocated_mount_config(config.release());

  return options;
}

authpolicy::KerberosFiles CreateKerberosFilesProto(
    const std::string& krb5cc, const std::string& krb5conf) {
  authpolicy::KerberosFiles kerberos_files;
  kerberos_files.set_krb5cc(krb5cc);
  kerberos_files.set_krb5conf(krb5conf);
  return kerberos_files;
}

UpdateMountCredentialsOptionsProto CreateUpdateMountCredentialsOptionsProto(
    int32_t mount_id,
    const std::string& workgroup,
    const std::string& username) {
  UpdateMountCredentialsOptionsProto update_options;
  update_options.set_mount_id(mount_id);
  update_options.set_workgroup(workgroup);
  update_options.set_username(username);

  return update_options;
}

PremountOptionsProto CreatePremountOptionsProto(const std::string& path) {
  PremountOptionsProto premount_options;
  premount_options.set_path(path);

  // Default to enable NTLM authentication.
  std::unique_ptr<MountConfigProto> config =
      CreateMountConfigProto(true /* enable_ntlm */);

  premount_options.set_allocated_mount_config(config.release());

  return premount_options;
}

UpdateSharePathOptionsProto CreateUpdateSharePathOptionsProto(
    int32_t mount_id, const std::string& share_path) {
  UpdateSharePathOptionsProto update_share_path_options;
  update_share_path_options.set_mount_id(mount_id);
  update_share_path_options.set_path(share_path);

  return update_share_path_options;
}

ProtoBlob CreateMountOptionsBlob(const std::string& path) {
  return SerializeProtoToBlobAndCheck(
      CreateMountOptionsProto(path, "" /* workgroup */, "" /* username */,
                              MountConfig(true /* enable_ntlm */)));
}

ProtoBlob CreateMountOptionsBlob(const std::string& path,
                                 const MountConfig& mount_config) {
  return SerializeProtoToBlobAndCheck(CreateMountOptionsProto(
      path, "" /* workgroup */, "" /* username */, mount_config));
}

ProtoBlob CreateMountOptionsBlob(const std::string& path,
                                 const std::string& workgroup,
                                 const std::string& username,
                                 const MountConfig& mount_config) {
  return SerializeProtoToBlobAndCheck(
      CreateMountOptionsProto(path, workgroup, username, mount_config));
}

ProtoBlob CreateUnmountOptionsBlob(int32_t mount_id) {
  return SerializeProtoToBlobAndCheck(CreateUnmountOptionsProto(mount_id));
}

ProtoBlob CreateReadDirectoryOptionsBlob(int32_t mount_id,
                                         const std::string& directory_path) {
  return SerializeProtoToBlobAndCheck(
      CreateReadDirectoryOptionsProto(mount_id, directory_path));
}

ProtoBlob CreateGetMetadataOptionsBlob(int32_t mount_id,
                                       const std::string& entry_path) {
  ProtoBlob proto_blob;
  return SerializeProtoToBlobAndCheck(
      CreateGetMetadataOptionsProto(mount_id, entry_path));
}

ProtoBlob CreateOpenFileOptionsBlob(int32_t mount_id,
                                    const std::string& file_path,
                                    bool writeable) {
  return SerializeProtoToBlobAndCheck(
      CreateOpenFileOptionsProto(mount_id, file_path, writeable));
}

ProtoBlob CreateCloseFileOptionsBlob(int32_t mount_id, int32_t file_id) {
  return SerializeProtoToBlobAndCheck(
      CreateCloseFileOptionsProto(mount_id, file_id));
}

ProtoBlob CreateDeleteEntryOptionsBlob(int32_t mount_id,
                                       const std::string& entry_path,
                                       bool recursive) {
  return SerializeProtoToBlobAndCheck(
      CreateDeleteEntryOptionsProto(mount_id, entry_path, recursive));
}

ProtoBlob CreateReadFileOptionsBlob(int32_t mount_id,
                                    int32_t file_id,
                                    int64_t offset,
                                    int32_t length) {
  return SerializeProtoToBlobAndCheck(
      CreateReadFileOptionsProto(mount_id, file_id, offset, length));
}

ProtoBlob CreateCreateFileOptionsBlob(int32_t mount_id,
                                      const std::string& file_path) {
  return SerializeProtoToBlobAndCheck(
      CreateCreateFileOptionsProto(mount_id, file_path));
}

ProtoBlob CreateTruncateOptionsBlob(int32_t mount_id,
                                    const std::string& file_path,
                                    int64_t length) {
  return SerializeProtoToBlobAndCheck(
      CreateTruncateOptionsProto(mount_id, file_path, length));
}

ProtoBlob CreateWriteFileOptionsBlob(int32_t mount_id,
                                     int32_t file_id,
                                     int64_t offset,
                                     int32_t length) {
  return SerializeProtoToBlobAndCheck(
      CreateWriteFileOptionsProto(mount_id, file_id, offset, length));
}

ProtoBlob CreateCreateDirectoryOptionsBlob(int32_t mount_id,
                                           const std::string& directory_path,
                                           bool recursive) {
  return SerializeProtoToBlobAndCheck(
      CreateCreateDirectoryOptionsProto(mount_id, directory_path, recursive));
}

ProtoBlob CreateMoveEntryOptionsBlob(int32_t mount_id,
                                     const std::string& source_path,
                                     const std::string& target_path) {
  return SerializeProtoToBlobAndCheck(
      CreateMoveEntryOptionsProto(mount_id, source_path, target_path));
}

ProtoBlob CreateCopyEntryOptionsBlob(int32_t mount_id,
                                     const std::string& source_path,
                                     const std::string& target_path) {
  return SerializeProtoToBlobAndCheck(
      CreateCopyEntryOptionsProto(mount_id, source_path, target_path));
}

ProtoBlob CreateGetDeleteListOptionsBlob(int32_t mount_id,
                                         const std::string& entry_path) {
  return SerializeProtoToBlobAndCheck(
      CreateGetDeleteListOptionsProto(mount_id, entry_path));
}

ProtoBlob CreateGetSharesOptionsBlob(const std::string& server_url) {
  return SerializeProtoToBlobAndCheck(CreateGetSharesOptionsProto(server_url));
}

ProtoBlob CreateRemountOptionsBlob(const std::string& path,
                                   const std::string& workgroup,
                                   const std::string& username,
                                   int32_t mount_id,
                                   MountConfig mount_config) {
  return SerializeProtoToBlobAndCheck(CreateRemountOptionsProto(
      path, workgroup, username, mount_id, std::move(mount_config)));
}

ProtoBlob CreateUpdateMountCredentialsOptionsBlob(int32_t mount_id,
                                                  const std::string& workgroup,
                                                  const std::string& username) {
  return SerializeProtoToBlobAndCheck(
      CreateUpdateMountCredentialsOptionsProto(mount_id, workgroup, username));
}

ProtoBlob CreatePremountOptionsBlob(const std::string& path) {
  return SerializeProtoToBlobAndCheck(CreatePremountOptionsProto(path));
}

ProtoBlob CreateUpdateSharePathOptionsBlob(int32_t mount_id,
                                           const std::string& share_path) {
  return SerializeProtoToBlobAndCheck(
      CreateUpdateSharePathOptionsProto(mount_id, share_path));
}

base::ScopedFD WritePasswordToFile(TempFileManager* temp_manager,
                                   const std::string& password) {
  const size_t password_size = password.size();
  std::vector<uint8_t> password_data(sizeof(password_size) + password.size());

  // Write the password length in the first sizeof(size_t) bytes of the buffer.
  std::memcpy(password_data.data(), &password_size, sizeof(password_size));

  // Append |password| starting at the end of password_size.
  std::memcpy(password_data.data() + sizeof(password_size), password.c_str(),
              password.size());

  return temp_manager->CreateTempFile(password_data);
}

std::string CreateKrb5ConfPath(const base::FilePath& temp_dir) {
  return temp_dir.Append(kTestKrb5ConfName).value();
}

std::string CreateKrb5CCachePath(const base::FilePath& temp_dir) {
  return temp_dir.Append(kTestCCacheName).value();
}

void ExpectFileEqual(const std::string& path,
                     const std::string expected_contents) {
  const base::FilePath file_path(path);
  std::string actual_contents;
  EXPECT_TRUE(ReadFileToString(file_path, &actual_contents));

  EXPECT_EQ(expected_contents, actual_contents);
}

void ExpectFileNotEqual(const std::string& path,
                        const std::string expected_contents) {
  const base::FilePath file_path(path);
  std::string actual_contents;
  EXPECT_TRUE(ReadFileToString(file_path, &actual_contents));

  EXPECT_NE(expected_contents, actual_contents);
}

void ExpectCredentialsEqual(MountManager* mount_manager,
                            int32_t mount_id,
                            const std::string& root_path,
                            const std::string& workgroup,
                            const std::string& username,
                            const std::string& password) {
  DCHECK(mount_manager);

  constexpr size_t kComparisonBufferSize = 256;
  char workgroup_buffer[kComparisonBufferSize];
  char username_buffer[kComparisonBufferSize];
  char password_buffer[kComparisonBufferSize];

  SambaInterface* samba_interface;
  EXPECT_TRUE(mount_manager->GetSambaInterface(mount_id, &samba_interface));

  const SambaInterface::SambaInterfaceId samba_interface_id =
      samba_interface->GetSambaInterfaceId();

  EXPECT_TRUE(mount_manager->GetAuthentication(
      samba_interface_id, root_path, workgroup_buffer, kComparisonBufferSize,
      username_buffer, kComparisonBufferSize, password_buffer,
      kComparisonBufferSize));

  EXPECT_EQ(std::string(workgroup_buffer), workgroup);
  EXPECT_EQ(std::string(username_buffer), username);
  EXPECT_EQ(std::string(password_buffer), password);
}

std::vector<uint8_t> CreateNetBiosResponsePacket(
    const std::vector<std::vector<uint8_t>>& hostnames,
    uint8_t name_length,
    std::vector<uint8_t> name,
    uint16_t transaction_id,
    uint8_t response_type) {
  // Build the prefix of the packet.
  std::vector<uint8_t> packet = {0xAA, 0xAA, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Set Transaction ID in Big Endian representation.
  packet[0] = transaction_id >> 8;
  packet[1] = transaction_id & 0xFF;

  // Add the name section.
  packet.push_back(name_length);
  packet.insert(packet.end(), name.begin(), name.end());

  // Add the next section
  std::vector<uint8_t> middle_section = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00};
  // Set the response_type.
  middle_section[2] = response_type;

  packet.insert(packet.end(), middle_section.begin(), middle_section.end());

  // Set number of address list entries.
  packet.push_back(hostnames.size());

  // Add the address list entries.
  for (const auto& hostname : hostnames) {
    packet.insert(packet.end(), hostname.begin(), hostname.end());
  }

  return packet;
}

std::vector<uint8_t> CreateNetBiosResponsePacket(
    const std::vector<std::vector<uint8_t>>& hostnames,
    std::vector<uint8_t> name,
    uint16_t transaction_id,
    uint8_t response_type) {
  return CreateNetBiosResponsePacket(hostnames, name.size(), name,
                                     transaction_id, response_type);
}

std::vector<uint8_t> CreateValidNetBiosHostname(const std::string& hostname,
                                                uint8_t type) {
  DCHECK_LE(hostname.size(), netbios::kServerNameLength);

  std::vector<uint8_t> hostname_bytes(netbios::kServerEntrySize);
  std::copy(hostname.begin(), hostname.end(), hostname_bytes.begin());

  // Fill the rest of the name with spaces.
  std::fill(hostname_bytes.begin() + hostname.size(),
            hostname_bytes.begin() + netbios::kServerNameLength, 0x20);

  // Set the type.
  hostname_bytes[15] = type;

  // Set two nulls for the flags.
  hostname_bytes[16] = 0x00;
  hostname_bytes[17] = 0x00;

  return hostname_bytes;
}

std::unique_ptr<MountConfigProto> CreateMountConfigProto(bool enable_ntlm) {
  // set_allocated_mount_config() transfers ownership of |mount_config| to
  // |mount_options| so |mount_config| needs to be created in the heap.
  auto mount_config = std::make_unique<MountConfigProto>();
  mount_config->set_enable_ntlm(enable_ntlm);

  return mount_config;
}

}  // namespace smbprovider
