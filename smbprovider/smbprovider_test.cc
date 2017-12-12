// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "smbprovider/constants.h"
#include "smbprovider/fake_samba_interface.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/smbprovider.h"
#include "smbprovider/smbprovider_helper.h"

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
constexpr uint64_t kFileDate = 42;

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

  ProtoBlob CreateOpenFileOptionsBlob(int32_t mount_id,
                                      const std::string& file_path,
                                      bool writeable) {
    ProtoBlob proto_blob;
    OpenFileOptions open_file_options;
    open_file_options.set_mount_id(mount_id);
    open_file_options.set_file_path(file_path);
    open_file_options.set_writeable(writeable);
    EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(open_file_options, &proto_blob));
    return proto_blob;
  }

  ProtoBlob CreateCloseFileOptionsBlob(int32_t file_id) {
    ProtoBlob proto_blob;
    CloseFileOptions close_file_options;
    close_file_options.set_file_id(file_id);
    EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(close_file_options, &proto_blob));
    return proto_blob;
  }

  // Helper method that adds "smb://wdshare/test" as a mountable share and
  // mounts it.
  int32_t PrepareMount() {
    fake_samba_->AddDirectory("smb://wdshare");
    fake_samba_->AddDirectory("smb://wdshare/test");
    int32_t mount_id;
    int32_t err;
    ProtoBlob proto_blob = CreateMountOptionsBlob("smb://wdshare/test");
    smbprovider_->Mount(proto_blob, &err, &mount_id);
    EXPECT_EQ(ERROR_OK, CastError(err));
    ExpectNoOpenDirectories();
    return mount_id;
  }

  void SetSmbProviderBuffer(int32_t buffer_size) {
    std::unique_ptr<FakeSambaInterface> fake_ptr =
        std::make_unique<FakeSambaInterface>();
    fake_samba_ = fake_ptr.get();
    const ObjectPath object_path("/object/path");
    smbprovider_.reset(new SmbProvider(
        std::make_unique<DBusObject>(nullptr, mock_bus_, object_path),
        std::move(fake_ptr), buffer_size));
  }

  // Helper method that asserts there are no entries that have not been
  // closed.
  void ExpectNoOpenDirectories() {
    EXPECT_FALSE(fake_samba_->HasOpenEntries());
  }

  // Helper method that creates a CloseFileOptionsBlob for |file_id| and
  // calls SmbProvider::CloseFile with it, expecting success.
  void CloseFileHelper(int32_t file_id) {
    ProtoBlob close_file_blob = CreateCloseFileOptionsBlob(file_id);
    EXPECT_EQ(ERROR_OK, smbprovider_->CloseFile(close_file_blob));
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
  ProtoBlob proto_blob = CreateMountOptionsBlob("smb://test/invalid");
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
  fake_samba_->AddDirectory("smb://wdshare");
  fake_samba_->AddDirectory("smb://wdshare/dogs");
  fake_samba_->AddDirectory("smb://wdshare/cats");
  int32_t mount1_id = -1;
  int32_t mount2_id = -1;
  int32_t error;
  ProtoBlob proto_blob_1 = CreateMountOptionsBlob("smb://wdshare/dogs");
  ProtoBlob proto_blob_2 = CreateMountOptionsBlob("smb://wdshare/cats");
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
  fake_samba_->AddDirectory("smb://wdshare");
  fake_samba_->AddDirectory("smb://wdshare/dogs");
  int32_t mount_id1 = -1;
  int32_t error;
  ProtoBlob mount_options_blob1 = CreateMountOptionsBlob("smb://wdshare/dogs");
  smbprovider_->Mount(mount_options_blob1, &error, &mount_id1);

  ProtoBlob unmount_options_blob = CreateUnmountOptionsBlob(mount_id1);
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->Unmount(unmount_options_blob)));

  int32_t mount_id2 = -1;
  ProtoBlob mount_options_blob2 = CreateMountOptionsBlob("smb://wdshare/dogs");
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
  ProtoBlob read_directory_blob = CreateReadDirectoryOptionsBlob(
      999 /* mount_id */, "smb://wdshare/test/path");
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

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  ProtoBlob results;
  int32_t err;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path");
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

  fake_samba_->AddDirectory("smb://wdshare/test/path");

  const std::string file_path =
      AppendPath("smb://wdshare/test/path", "file.jpg");

  fake_samba_->AddFile(file_path, kFileSize);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path");
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
  size_t buffer_size = CalculateEntrySize("file1.jpg") + 1;

  SetSmbProviderBuffer(buffer_size);
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");

  fake_samba_->AddFile("smb://wdshare/test/path/file1.jpg", kFileSize);
  fake_samba_->AddFile("smb://wdshare/test/path/file2.jpg", kFileSize);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path");
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(2, entries.entries_size());

  const DirectoryEntry& entry1 = entries.entries(0);
  EXPECT_FALSE(entry1.is_directory());
  EXPECT_EQ("file1.jpg", entry1.name());

  const DirectoryEntry& entry2 = entries.entries(1);
  EXPECT_FALSE(entry2.is_directory());
  EXPECT_EQ("file2.jpg", entry2.name());
}

// Read directory succeeds and returns expected entries.
TEST_F(SmbProviderTest, ReadDirectorySucceedsWithNonEmptyDir) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");

  fake_samba_->AddFile("smb://wdshare/test/path/file.jpg", kFileSize);
  fake_samba_->AddDirectory("smb://wdshare/test/path/images");

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path");
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(2, entries.entries_size());

  const DirectoryEntry& entry1 = entries.entries(0);
  EXPECT_FALSE(entry1.is_directory());
  EXPECT_EQ("file.jpg", entry1.name());

  const DirectoryEntry& entry2 = entries.entries(1);
  EXPECT_TRUE(entry2.is_directory());
  EXPECT_EQ("images", entry2.name());
}

// Read directory succeeds and omits "." and ".." entries.
TEST_F(SmbProviderTest, ReadDirectoryDoesntReturnSelfAndParentEntries) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");

  fake_samba_->AddFile("smb://wdshare/test/path/file.jpg", kFileSize);
  fake_samba_->AddDirectory("smb://wdshare/test/path/.");
  fake_samba_->AddDirectory("smb://wdshare/test/path/..");

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path");
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(1, entries.entries_size());

  const DirectoryEntry& entry = entries.entries(0);
  EXPECT_FALSE(entry.is_directory());
  EXPECT_EQ("file.jpg", entry.name());
}

// Read directory fails when called on a file.
TEST_F(SmbProviderTest, ReadDirectoryFailsWithFile) {
  int32_t mount_id = PrepareMount();

  const std::string file_url = "smb://wdshare/test/path/file.jpg";
  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile(file_url);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path/file.jpg");
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  EXPECT_EQ(ERROR_NOT_A_DIRECTORY, CastError(error_code));
}

// Read directory fails when called on a non file.
TEST_F(SmbProviderTest, ReadDirectoryFailsWithNonFileNonDirectory) {
  int32_t mount_id = PrepareMount();

  const std::string printer("canon.cn");
  const std::string printer_url = "smb://wdshare/test/path/canon.cn";
  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddEntry(printer_url, SMBC_PRINTER_SHARE);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path/canon.cn");
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  EXPECT_EQ(ERROR_NOT_A_DIRECTORY, CastError(error_code));
}

// Read directory suceeds and omits non files / non directories.
TEST_F(SmbProviderTest, ReadDirectoryDoesNotReturnNonFileNonDir) {
  int32_t mount_id = PrepareMount();

  const std::string printer("canon.cn");
  const std::string printer_url = "smb://wdshare/test/path/canon.cn";
  const std::string file_url = "smb://wdshare/test/path/file.jpg";
  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile(file_url);
  fake_samba_->AddEntry(printer_url, SMBC_PRINTER_SHARE);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, "/path");
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryList entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(1, entries.entries_size());

  const DirectoryEntry& entry = entries.entries(0);
  EXPECT_FALSE(entry.is_directory());
  EXPECT_EQ("file.jpg", entry.name());
}

// GetMetadata fails on non files/dirs. Overall, other types
// are treated as if they do not exist.
TEST_F(SmbProviderTest, GetMetadataFailsWithNonFileNonDir) {
  int32_t mount_id = PrepareMount();

  const std::string printer("canon.cn");
  const std::string printer_url = "smb://wdshare/test/path/canon.cn";
  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddEntry(printer_url, SMBC_PRINTER_SHARE);

  ProtoBlob result;
  int32_t error_code;
  ProtoBlob get_metadata_blob =
      CreateGetMetadataOptionsBlob(mount_id, "/path/canon.cn");
  smbprovider_->GetMetadataEntry(get_metadata_blob, &error_code, &result);

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(error_code));
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
  ProtoBlob get_metadata_blob = CreateGetMetadataOptionsBlob(123, "/path");
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

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile("smb://wdshare/test/path/dog.jpg", kFileSize, kFileDate);

  ProtoBlob result;
  int32_t error_code;
  ProtoBlob get_metadata_blob =
      CreateGetMetadataOptionsBlob(mount_id, "/path/dog.jpg");
  smbprovider_->GetMetadataEntry(get_metadata_blob, &error_code, &result);

  DirectoryEntry entry;
  const std::string parsed_proto(result.begin(), result.end());
  EXPECT_TRUE(entry.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_FALSE(entry.is_directory());
  EXPECT_EQ("dog.jpg", entry.name());
  EXPECT_EQ(kFileSize, entry.size());
  EXPECT_EQ(kFileDate, entry.last_modified_time());
}

// OpenFile fails when called on a non existent file.
TEST_F(SmbProviderTest, OpenFileFailsFileDoesNotExist) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/dog.jpg", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_NOT_FOUND, error_code);
  EXPECT_EQ(-1, file_id);
}

// OpenFile succeeds and returns a valid file_id when called on a valid file.
TEST_F(SmbProviderTest, OpenFileSuceedsOnValidFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile("smb://wdshare/test/path/dog.jpg", kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/dog.jpg", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_OK, error_code);
  EXPECT_GE(file_id, 0);

  CloseFileHelper(file_id);
}

// OpenFile fails when called on a directory.
TEST_F(SmbProviderTest, OpenFileFailsOnDirectory) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob =
      CreateOpenFileOptionsBlob(mount_id, "/path", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_NOT_FOUND, error_code);
  EXPECT_EQ(-1, file_id);
}

// OpenFile fails when called on a non file / non directory.
TEST_F(SmbProviderTest, OpenFileFailsOnNonFileNonDir) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddEntry("smb://wdshare/test/path/canon.cn", SMBC_PRINTER_SHARE);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/canon.cn", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_NOT_FOUND, error_code);
  EXPECT_EQ(-1, file_id);
}

// OpenFile sets read and write flags correctly in the corresponding OpenInfo.
TEST_F(SmbProviderTest, OpenFileReadandWriteFlagSetCorrectly) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile("smb://wdshare/test/path/dog.jpg", kFileSize, kFileDate);
  fake_samba_->AddFile("smb://wdshare/test/path/cat.jpg", kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/dog.jpg", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  int32_t file_id_2;
  int32_t error_code_2;
  open_file_blob = CreateOpenFileOptionsBlob(mount_id, "/path/cat.jpg",
                                             true /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code_2, &file_id_2);

  EXPECT_EQ(ERROR_OK, error_code);
  EXPECT_TRUE(fake_samba_->HasReadSet(file_id));
  EXPECT_FALSE(fake_samba_->HasWriteSet(file_id));

  EXPECT_EQ(ERROR_OK, error_code_2);
  EXPECT_TRUE(fake_samba_->HasReadSet(file_id_2));
  EXPECT_TRUE(fake_samba_->HasWriteSet(file_id_2));

  CloseFileHelper(file_id);
  CloseFileHelper(file_id_2);
}

// CloseFile succeeds on a valid file.
TEST_F(SmbProviderTest, CloseFileSuceedsOnOpenFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile("smb://wdshare/test/path/dog.jpg", kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/dog.jpg", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);
  EXPECT_EQ(ERROR_OK, error_code);

  CloseFileHelper(file_id);
}

// CloseFile closes the correct file when multiple files are open.
TEST_F(SmbProviderTest, CloseFileClosesCorrectFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile("smb://wdshare/test/path/dog.jpg", kFileSize, kFileDate);
  fake_samba_->AddFile("smb://wdshare/test/path/cat.jpg", kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/dog.jpg", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  int32_t file_id_2;
  int32_t error_code_2;
  open_file_blob = CreateOpenFileOptionsBlob(mount_id, "/path/cat.jpg",
                                             false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code_2, &file_id_2);

  EXPECT_TRUE(fake_samba_->IsFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFDOpen(file_id_2));
  EXPECT_NE(file_id, file_id_2);

  CloseFileHelper(file_id);
  EXPECT_FALSE(fake_samba_->IsFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFDOpen(file_id_2));

  CloseFileHelper(file_id_2);
}

// CloseFile closes the correct instance of a file with opened more than once.
TEST_F(SmbProviderTest, CloseFileClosesCorrectInstanceOfSameFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile("smb://wdshare/test/path/dog.jpg", kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/dog.jpg", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  int32_t file_id_2;
  int32_t error_code_2;
  open_file_blob = CreateOpenFileOptionsBlob(mount_id, "/path/dog.jpg",
                                             false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code_2, &file_id_2);

  EXPECT_TRUE(fake_samba_->IsFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFDOpen(file_id_2));
  EXPECT_NE(file_id, file_id_2);

  CloseFileHelper(file_id);
  EXPECT_FALSE(fake_samba_->IsFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFDOpen(file_id_2));

  CloseFileHelper(file_id_2);
}

// CloseFile fails when called on a closed file.
TEST_F(SmbProviderTest, CloseFileFailsWhenFileIsNotOpen) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory("smb://wdshare/test/path");
  fake_samba_->AddFile("smb://wdshare/test/path/dog.jpg", kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, "/path/dog.jpg", false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);
  EXPECT_EQ(ERROR_OK, error_code);
  CloseFileHelper(file_id);

  ProtoBlob close_file_blob = CreateCloseFileOptionsBlob(file_id);
  EXPECT_EQ(ERROR_NOT_FOUND, smbprovider_->CloseFile(close_file_blob));
}

// CloseFile fails when called with a non-existant file handler.
TEST_F(SmbProviderTest, TestName) {
  ProtoBlob close_file_blob = CreateCloseFileOptionsBlob(1564);

  EXPECT_EQ(ERROR_NOT_FOUND, smbprovider_->CloseFile(close_file_blob));
}

}  // namespace smbprovider
