// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/smbprovider.h"
#include "smbprovider/constants.h"
#include "smbprovider/fake_samba_interface.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/smbprovider_helper.h"

#include <base/memory/ptr_util.h>
#include <dbus/mock_bus.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>
#include <utility>

using brillo::dbus_utils::DBusObject;
using dbus::MockBus;
using dbus::ObjectPath;

namespace smbprovider {

namespace {

constexpr size_t kFileSize = 10;
constexpr char kValidPath[] = "/path/valid";
constexpr char kValidSharePath[] = "smb://wdshare/test";

ErrorType CastError(int error) {
  EXPECT_GE(error, 0);
  return static_cast<ErrorType>(error);
}

}  // namespace

class SmbProviderTest : public testing::Test {
 public:
  SmbProviderTest() { SetSmbProviderBuffer(kBufferSize); }

 protected:
  using DirEntries = std::vector<smbc_dirent>;

  ProtoBlob CreateMountOptionsBlob(const std::string& path) {
    ProtoBlob proto_blob;
    MountOptions mount_options;
    mount_options.set_path(path);
    EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(mount_options, &proto_blob));
    return proto_blob;
  }

  ProtoBlob CreateUnmountOptionsBlob(int32_t mount_id) {
    ProtoBlob proto_blob;
    UnmountOptions unmount_options;
    unmount_options.set_mount_id(mount_id);
    EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(unmount_options, &proto_blob));
    return proto_blob;
  }

  ProtoBlob CreateReadDirectoryOptionsBlob(int32_t mount_id,
                                           const std::string& directory_path) {
    ProtoBlob proto_blob;
    ReadDirectoryOptions read_directory_options;
    read_directory_options.set_mount_id(mount_id);
    read_directory_options.set_directory_path(directory_path);
    EXPECT_EQ(ERROR_OK,
              SerializeProtoToBlob(read_directory_options, &proto_blob));
    return proto_blob;
  }

  ProtoBlob CreateGetMetadataOptionsBlob(int32_t mount_id,
                                         const std::string& entry_path) {
    ProtoBlob proto_blob;
    GetMetadataEntryOptions get_metadata_options;
    get_metadata_options.set_mount_id(mount_id);
    get_metadata_options.set_entry_path(entry_path);
    EXPECT_EQ(ERROR_OK,
              SerializeProtoToBlob(get_metadata_options, &proto_blob));
    return proto_blob;
  }

  // Helper method that adds |kValidSharePath| as a mountable share and mounts
  // it.
  int32_t PrepareMount() {
    fake_samba_->AddDirectory(std::string(kValidSharePath));
    int32_t mount_id;
    int32_t err;
    ProtoBlob proto_blob = CreateMountOptionsBlob(kValidSharePath);
    smbprovider_->Mount(proto_blob, &err, &mount_id);
    EXPECT_EQ(ERROR_OK, CastError(err));
    ExpectNoOpenDirectories();
    return mount_id;
  }

  void SetSmbProviderBuffer(int32_t buffer_size) {
    std::unique_ptr<FakeSambaInterface> fake_ptr =
        base::MakeUnique<FakeSambaInterface>();
    fake_samba_ = fake_ptr.get();
    const ObjectPath object_path(std::string("/object/path"));
    smbprovider_.reset(new SmbProvider(
        base::MakeUnique<DBusObject>(nullptr, mock_bus_, object_path),
        std::move(fake_ptr), buffer_size));
  }

  // Helper method that asserts there are no entries that have not been
  // closed.
  void ExpectNoOpenDirectories() {
    EXPECT_FALSE(fake_samba_->HasOpenDirectories());
  }

  scoped_refptr<MockBus> mock_bus_ = new MockBus(dbus::Bus::Options());
  std::unique_ptr<SmbProvider> smbprovider_;
  FakeSambaInterface* fake_samba_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbProviderTest);
};

// Should properly serialize protobuf.
TEST_F(SmbProviderTest, ShouldSerializeProto) {
  const std::string path("smb://192.168.0.1/test");
  MountOptions mount_options;
  mount_options.set_path(path);
  ProtoBlob buffer;
  EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(mount_options, &buffer));
  EXPECT_EQ(mount_options.ByteSizeLong(), buffer.size());

  MountOptions deserialized_proto;
  EXPECT_TRUE(deserialized_proto.ParseFromArray(buffer.data(), buffer.size()));
  EXPECT_EQ(path, deserialized_proto.path());
}

// Mount fails when an invalid protobuf with missing fields is passed.
TEST_F(SmbProviderTest, MountFailsWithInvalidProto) {
  int32_t mount_id;
  int32_t err;
  ProtoBlob empty_blob;
  smbprovider_->Mount(empty_blob, &err, &mount_id);
  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED, CastError(err));
}

// Mount fails when mounting a share that doesn't exist.
TEST_F(SmbProviderTest, MountFailsWithInvalidShare) {
  int32_t mount_id;
  int32_t err;
  ProtoBlob proto_blob = CreateMountOptionsBlob("/test/invalid");
  smbprovider_->Mount(proto_blob, &err, &mount_id);
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(err));
  ExpectNoOpenDirectories();
}

// Unmount fails when an invalid protobuf with missing fields is passed.
TEST_F(SmbProviderTest, UnmountFailsWithInvalidProto) {
  ProtoBlob empty_blob;
  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED,
            CastError(smbprovider_->Unmount(empty_blob)));
}

// Unmount fails when unmounting an |mount_id| that wasn't previously mounted.
TEST_F(SmbProviderTest, UnmountFailsWithUnmountedShare) {
  ProtoBlob proto_blob = CreateUnmountOptionsBlob(123);
  int32_t error = smbprovider_->Unmount(proto_blob);
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(error));
  ExpectNoOpenDirectories();
}

// Mounting different shares should return different mount ids.
TEST_F(SmbProviderTest, MountReturnsDifferentMountIds) {
  const std::string share1("smb://wdshare/dogs");
  const std::string share2("smb://wdshare/cats");
  fake_samba_->AddDirectory(share1);
  fake_samba_->AddDirectory(share2);
  int32_t mount1_id = -1;
  int32_t mount2_id = -1;
  int32_t error;
  ProtoBlob proto_blob_1 = CreateMountOptionsBlob(share1);
  ProtoBlob proto_blob_2 = CreateMountOptionsBlob(share2);
  smbprovider_->Mount(proto_blob_1, &error, &mount1_id);
  smbprovider_->Mount(proto_blob_2, &error, &mount2_id);
  EXPECT_NE(mount1_id, mount2_id);
}

// Mount and unmount succeed when mounting a valid share path and unmounting
// using the |mount_id| from |Mount|.
TEST_F(SmbProviderTest, MountUnmountSucceedsWithValidShare) {
  int32_t mount_id = PrepareMount();
  EXPECT_EQ(0, mount_id);
  ExpectNoOpenDirectories();

  ProtoBlob proto_blob = CreateUnmountOptionsBlob(mount_id);
  int32_t error = smbprovider_->Unmount(proto_blob);
  EXPECT_EQ(ERROR_OK, CastError(error));
  ExpectNoOpenDirectories();
}

// Mount ids should not be reused.
TEST_F(SmbProviderTest, MountIdsDontGetReused) {
  const std::string share("smb://wdshare/dogs");
  fake_samba_->AddDirectory(share);
  int32_t mount_id1 = -1;
  int32_t error;
  ProtoBlob mount_options_blob1 = CreateMountOptionsBlob(share);
  smbprovider_->Mount(mount_options_blob1, &error, &mount_id1);

  ProtoBlob unmount_options_blob = CreateUnmountOptionsBlob(mount_id1);
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->Unmount(unmount_options_blob)));

  int32_t mount_id2 = -1;
  ProtoBlob mount_options_blob2 = CreateMountOptionsBlob(share);
  smbprovider_->Mount(mount_options_blob2, &error, &mount_id2);
  EXPECT_NE(mount_id1, mount_id2);
}

// ReadDirectory fails when an invalid protobuf with missing fields is passed.
TEST_F(SmbProviderTest, ReadDirectoryFailsWithInvalidProto) {
  int32_t err;
  ProtoBlob results;
  ProtoBlob empty_blob;
  smbprovider_->ReadDirectory(empty_blob, &err, &results);
  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED, CastError(err));
  EXPECT_TRUE(results.empty());
}

// ReadDirectory fails when passed a |mount_id| that wasn't previously mounted.
TEST_F(SmbProviderTest, ReadDirectoryFailsWithUnmountedShare) {
  ProtoBlob results;
  int32_t err;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(999, kValidPath);
  smbprovider_->ReadDirectory(read_directory_blob, &err, &results);
  EXPECT_TRUE(results.empty());
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(err));
  ExpectNoOpenDirectories();
}

// Read directory fails when passed a path that doesn't exist.
TEST_F(SmbProviderTest, ReadDirectoryFailsWithInvalidDir) {
  int32_t mount_id = PrepareMount();

  ProtoBlob results;
  int32_t err;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/test/invalid");
  smbprovider_->ReadDirectory(read_directory_blob, &err, &results);
  EXPECT_TRUE(results.empty());
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(err));
}

// ReadDirectory succeeds when reading an empty directory.
TEST_F(SmbProviderTest, ReadDirectorySucceedsWithEmptyDir) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(
      AppendPath(std::string(kValidSharePath), std::string(kValidPath)));

  ProtoBlob results;
  int32_t err;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, kValidPath);
  smbprovider_->ReadDirectory(read_directory_blob, &err, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(err));
  EXPECT_EQ(0, entries.entries_size());
  ExpectNoOpenDirectories();
}

// Read directory succeeds but does not return files when it exceeds the buffer
// size.
TEST_F(SmbProviderTest, ReadDirectoryDoesNotReturnEntryWithSmallBuffer) {
  // Construct smbprovider_ with buffer size of 1.
  SetSmbProviderBuffer(1);
  int32_t mount_id = PrepareMount();

  int32_t dir_id = fake_samba_->AddDirectory(
      AppendPath(std::string(kValidSharePath), std::string(kValidPath)));
  const std::string file("file.jpg");
  fake_samba_->AddEntry(dir_id, file, SMBC_FILE, kFileSize);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, kValidPath);
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(0, entries.entries_size());
}

// Read directory succeeds when the buffer size is just big enough for one
// entry. This should return one entry, and ReadDirectory will loop through the
// buffer again from the beginning.
TEST_F(SmbProviderTest, ReadDirectorySucceedsWithMultipleUsageOfSmallBuffer) {
  const std::string file_name1("file1.jpg");
  const std::string file_name2("file2.jpg");
  size_t buffer_size = CalculateEntrySize(file_name1) + 1;

  SetSmbProviderBuffer(buffer_size);
  int32_t mount_id = PrepareMount();

  int32_t dir_id = fake_samba_->AddDirectory(
      AppendPath(std::string(kValidSharePath), std::string(kValidPath)));
  fake_samba_->AddEntry(dir_id, file_name1, SMBC_FILE, kFileSize);
  fake_samba_->AddEntry(dir_id, file_name2, SMBC_FILE, kFileSize);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, kValidPath);
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(2, entries.entries_size());

  const DirectoryEntry& entry1 = entries.entries(0);
  EXPECT_FALSE(entry1.is_directory());
  EXPECT_EQ(file_name1, entry1.name());

  const DirectoryEntry& entry2 = entries.entries(1);
  EXPECT_FALSE(entry2.is_directory());
  EXPECT_EQ(file_name2, entry2.name());
}

// Read directory succeeds and returns expected entries.
TEST_F(SmbProviderTest, ReadDirectorySucceedsWithNonEmptyDir) {
  int32_t mount_id = PrepareMount();

  int32_t dir_id = fake_samba_->AddDirectory(
      AppendPath(std::string(kValidSharePath), std::string(kValidPath)));
  const std::string file("file.jpg");
  const std::string dir("images");
  fake_samba_->AddEntry(dir_id, file, SMBC_FILE, kFileSize);
  fake_samba_->AddEntry(dir_id, dir, SMBC_DIR, 0);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, kValidPath);
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(2, entries.entries_size());

  const DirectoryEntry& entry1 = entries.entries(0);
  EXPECT_FALSE(entry1.is_directory());
  EXPECT_EQ(file, entry1.name());

  const DirectoryEntry& entry2 = entries.entries(1);
  EXPECT_TRUE(entry2.is_directory());
  EXPECT_EQ(dir, entry2.name());
}

// Read directory succeeds and omits "." and ".." entries.
TEST_F(SmbProviderTest, ReadDirectoryDoesntReturnSelfAndParentEntries) {
  int32_t mount_id = PrepareMount();

  int32_t dir_id = fake_samba_->AddDirectory(
      AppendPath(std::string(kValidSharePath), std::string(kValidPath)));
  const std::string file("file.jpg");
  fake_samba_->AddEntry(dir_id, file, SMBC_FILE, kFileSize);
  fake_samba_->AddEntry(dir_id, std::string(kEntrySelf), SMBC_DIR, 0);
  fake_samba_->AddEntry(dir_id, std::string(kEntryParent), SMBC_DIR, 0);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, kValidPath);
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(1, entries.entries_size());

  const DirectoryEntry& entry = entries.entries(0);
  EXPECT_FALSE(entry.is_directory());
  EXPECT_EQ(file, entry.name());
}

// GetMetadata fails when an invalid protobuf with missing fields is passed.
TEST_F(SmbProviderTest, GetMetadataFailsWithInvalidProto) {
  int32_t err;
  ProtoBlob result;
  ProtoBlob empty_blob;
  smbprovider_->GetMetadataEntry(empty_blob, &err, &result);
  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED, CastError(err));
  EXPECT_TRUE(result.empty());
}

// GetMetadata fails with when passed a |mount_id| that wasn't previously
// mounted.
TEST_F(SmbProviderTest, GetMetadataFailsWithUnmountedShare) {
  int32_t err;
  ProtoBlob result;
  ProtoBlob get_metadata_blob = CreateGetMetadataOptionsBlob(123, kValidPath);
  smbprovider_->GetMetadataEntry(get_metadata_blob, &err, &result);
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(err));
}

// GetMetadata fails when passed a path that doesn't exist.
TEST_F(SmbProviderTest, GetMetadataFailsWithInvalidPath) {
  int32_t mount_id = PrepareMount();

  ProtoBlob result;
  int32_t error_code;
  ProtoBlob get_metadata_blob =
      CreateGetMetadataOptionsBlob(mount_id, "/test/invalid");
  smbprovider_->GetMetadataEntry(get_metadata_blob, &error_code, &result);
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(error_code));
}

// GetMetadata succeeds when passed a valid share path.
TEST_F(SmbProviderTest, GetMetadataSucceeds) {
  int32_t mount_id = PrepareMount();
  const std::string dir_path = AppendPath(kValidSharePath, kValidPath);
  const std::string name("dog.jpg");
  // Add an entry path since chrome passes us entry paths with a leading '/'.
  const std::string entry_path = "/" + name;

  int32_t dir_id = fake_samba_->AddDirectory(dir_path);
  fake_samba_->AddEntry(dir_id, name, SMBC_FILE, kFileSize);

  ProtoBlob result;
  int32_t error_code;
  ProtoBlob get_metadata_blob = CreateGetMetadataOptionsBlob(
      mount_id, AppendPath(kValidPath, entry_path));
  smbprovider_->GetMetadataEntry(get_metadata_blob, &error_code, &result);

  DirectoryEntry entry;
  const std::string parsed_proto(result.begin(), result.end());
  EXPECT_TRUE(entry.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_FALSE(entry.is_directory());
  EXPECT_EQ(name, entry.name());
  EXPECT_EQ(kFileSize, entry.size());
}

}  // namespace smbprovider
