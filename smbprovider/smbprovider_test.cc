// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include <dbus/mock_bus.h>
#include <dbus/object_path.h>
#include <gtest/gtest.h>

#include "smbprovider/constants.h"
#include "smbprovider/fake_samba_interface.h"
#include "smbprovider/mount_manager.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/smbprovider.h"
#include "smbprovider/smbprovider_helper.h"
#include "smbprovider/smbprovider_test_helper.h"

using brillo::dbus_utils::DBusObject;

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

  // Helper method that adds "smb://wdshare/test" as a mountable share and
  // mounts it.
  int32_t PrepareMount() {
    fake_samba_->AddDirectory(GetDefaultServer());
    fake_samba_->AddDirectory(GetDefaultMountRoot());
    int32_t mount_id;
    int32_t err;
    ProtoBlob proto_blob = CreateMountOptionsBlob(GetDefaultMountRoot());
    smbprovider_->Mount(proto_blob, &err, &mount_id);
    EXPECT_EQ(ERROR_OK, CastError(err));
    ExpectNoOpenEntries();
    return mount_id;
  }

  // Helper method that calls PrepareMount() and adds a single directory with a
  // single file in the mount.
  int32_t PrepareSingleFileMount() {
    const int32_t mount_id = PrepareMount();
    fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
    fake_samba_->AddFile(GetAddedFullFilePath());
    return mount_id;
  }

  // Helper method that calls PrepareMount() and adds a single directory with a
  // single file in the mount. |file_data| is the data that will be in the file.
  int32_t PrepareSingleFileMountWithData(std::vector<uint8_t> file_data) {
    const int32_t mount_id = PrepareMount();
    fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
    fake_samba_->AddFile(GetAddedFullFilePath(), kFileDate,
                         std::move(file_data));
    return mount_id;
  }

  // Helper method that opens an already added file located in
  // GetDefaultFilePath(). PrepareSingleFileMount() or
  // PrepareSingleFileMountWithData() must be called beforehand.
  int32_t OpenAddedFile(int32_t mount_id) {
    return OpenAddedFile(mount_id, GetDefaultFilePath());
  }

  // Helper method that opens an already added file located in |path|.
  // PrepareSingleFileMount() or PrepareSingleFileMountWithData() must be called
  // beforehand.
  int32_t OpenAddedFile(int32_t mount_id, const std::string& path) {
    return OpenAddedFile(mount_id, path, false);
  }

  // Helper method that opens an already added file located in |path|.
  // PrepareSingleFileMount() or PrepareSingleFileMountWithData() must be called
  // beforehand. Permissions will be O_RDWR if |writeable| is true, otherwise it
  // will be O_RDONLY.
  int32_t OpenAddedFile(int32_t mount_id,
                        const std::string& path,
                        bool writeable) {
    int32_t file_id;
    int32_t error_code;
    ProtoBlob open_file_blob =
        CreateOpenFileOptionsBlob(mount_id, path, writeable);
    smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);
    DCHECK_EQ(static_cast<int32_t>(ERROR_OK), error_code);
    return file_id;
  }

  // Helper method that opens an already added directory located in |path|.
  // Returns the directory id.
  int32_t OpenAddedDirectory(const std::string& path) {
    int32_t dir_id;
    int32_t error_code = fake_samba_->OpenDirectory(path, &dir_id);
    DCHECK_EQ(0, error_code);
    return dir_id;
  }

  void SetSmbProviderBuffer(int32_t buffer_size) {
    std::unique_ptr<FakeSambaInterface> fake_ptr =
        std::make_unique<FakeSambaInterface>();
    fake_samba_ = fake_ptr.get();
    std::unique_ptr<MountManager> mount_manager_ptr =
        std::make_unique<MountManager>();
    mount_manager_ = mount_manager_ptr.get();
    const dbus::ObjectPath object_path("/object/path");
    smbprovider_ = std::make_unique<SmbProvider>(
        std::make_unique<DBusObject>(nullptr, mock_bus_, object_path),
        std::move(fake_ptr), std::move(mount_manager_ptr), buffer_size);
  }

  // Helper method that asserts there are no entries that have not been
  // closed.
  void ExpectNoOpenEntries() { EXPECT_FALSE(fake_samba_->HasOpenEntries()); }

  // Helper method that creates a CloseFileOptionsBlob for |file_id| and
  // calls SmbProvider::CloseFile with it, expecting success.
  void CloseFileHelper(int32_t mount_id, int32_t file_id) {
    ProtoBlob close_file_blob = CreateCloseFileOptionsBlob(mount_id, file_id);
    EXPECT_EQ(ERROR_OK, smbprovider_->CloseFile(close_file_blob));
  }

  // Helper method to read a file using the given options, and outputs a file
  // descriptor |fd|.
  void ReadFile(int32_t mount_id,
                int32_t file_id,
                int64_t offset,
                int32_t length,
                dbus::FileDescriptor* fd) {
    int32_t err;
    ProtoBlob read_file_blob =
        CreateReadFileOptionsBlob(mount_id, file_id, offset, length);
    smbprovider_->ReadFile(read_file_blob, &err, fd);
    EXPECT_EQ(ERROR_OK, CastError(err));
  }

  void ValidateFDContent(
      const dbus::FileDescriptor& fd,
      int32_t length_to_read,
      std::vector<uint8_t>::const_iterator data_start_iterator,
      std::vector<uint8_t>::const_iterator data_end_iterator) {
    EXPECT_EQ(length_to_read,
              std::distance(data_start_iterator, data_end_iterator));
    std::vector<uint8_t> buffer(length_to_read);
    EXPECT_TRUE(base::ReadFromFD(
        fd.value(), reinterpret_cast<char*>(buffer.data()), buffer.size()));
    EXPECT_TRUE(std::equal(data_start_iterator, data_end_iterator,
                           buffer.begin(), buffer.end()));
  }

  void WriteToTempFileWithData(const std::vector<uint8_t>& data,
                               dbus::FileDescriptor* fd) {
    EXPECT_FALSE(fd->is_valid());
    fd->PutValue(temp_file_manager_.CreateTempFile(data).release());
    fd->CheckValidity();
    EXPECT_TRUE(fd->is_valid());
  }

  scoped_refptr<dbus::MockBus> mock_bus_ =
      new dbus::MockBus(dbus::Bus::Options());
  std::unique_ptr<SmbProvider> smbprovider_;
  FakeSambaInterface* fake_samba_;
  MountManager* mount_manager_;
  TempFileManager temp_file_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbProviderTest);
};

// Should properly serialize protobuf.
TEST_F(SmbProviderTest, ShouldSerializeProto) {
  const std::string path("smb://192.168.0.1/test");
  MountOptionsProto mount_options;
  mount_options.set_path(path);
  ProtoBlob buffer;
  EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(mount_options, &buffer));
  EXPECT_EQ(mount_options.ByteSizeLong(), buffer.size());

  MountOptionsProto deserialized_proto;
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
  EXPECT_EQ(0, mount_manager_->MountCount());
  ExpectNoOpenEntries();
}

// Mount fails when mounting a share that doesn't exist.
TEST_F(SmbProviderTest, MountFailsWithInvalidShare) {
  int32_t mount_id;
  int32_t err;
  ProtoBlob proto_blob = CreateMountOptionsBlob("smb://test/invalid");
  smbprovider_->Mount(proto_blob, &err, &mount_id);
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(err));
  EXPECT_EQ(0, mount_manager_->MountCount());
  ExpectNoOpenEntries();
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
  ExpectNoOpenEntries();
}

// Mounting different shares should return different mount ids.
TEST_F(SmbProviderTest, MountReturnsDifferentMountIds) {
  fake_samba_->AddDirectory("smb://wdshare");
  fake_samba_->AddDirectory("smb://wdshare/dogs");
  fake_samba_->AddDirectory("smb://wdshare/cats");

  int32_t error;
  int32_t mount1_id;
  ProtoBlob proto_blob_1 = CreateMountOptionsBlob("smb://wdshare/dogs");
  smbprovider_->Mount(proto_blob_1, &error, &mount1_id);
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(1, mount_manager_->MountCount());
  EXPECT_TRUE(mount_manager_->IsAlreadyMounted(mount1_id));

  int32_t mount2_id;
  ProtoBlob proto_blob_2 = CreateMountOptionsBlob("smb://wdshare/cats");
  smbprovider_->Mount(proto_blob_2, &error, &mount2_id);
  EXPECT_EQ(ERROR_OK, error);
  EXPECT_EQ(2, mount_manager_->MountCount());
  EXPECT_TRUE(mount_manager_->IsAlreadyMounted(mount2_id));

  EXPECT_NE(mount1_id, mount2_id);
}

// Mount and unmount succeed when mounting a valid share path and unmounting
// using the |mount_id| from |Mount|.
TEST_F(SmbProviderTest, MountUnmountSucceedsWithValidShare) {
  int32_t mount_id = PrepareMount();
  EXPECT_GE(mount_id, 0);
  ExpectNoOpenEntries();
  EXPECT_EQ(1, mount_manager_->MountCount());
  EXPECT_TRUE(mount_manager_->IsAlreadyMounted(mount_id));

  ProtoBlob proto_blob = CreateUnmountOptionsBlob(mount_id);
  int32_t error = smbprovider_->Unmount(proto_blob);
  EXPECT_EQ(ERROR_OK, CastError(error));
  ExpectNoOpenEntries();
  EXPECT_EQ(0, mount_manager_->MountCount());
  EXPECT_FALSE(mount_manager_->IsAlreadyMounted(mount_id));
}

// Mount ids should not be reused.
TEST_F(SmbProviderTest, MountIdsDontGetReused) {
  fake_samba_->AddDirectory("smb://wdshare");
  fake_samba_->AddDirectory("smb://wdshare/dogs");

  // Create the first mount.
  int32_t error;
  int32_t mount_id_1;
  ProtoBlob mount_options_blob_1 = CreateMountOptionsBlob("smb://wdshare/dogs");
  smbprovider_->Mount(mount_options_blob_1, &error, &mount_id_1);
  EXPECT_EQ(1, mount_manager_->MountCount());
  EXPECT_TRUE(mount_manager_->IsAlreadyMounted(mount_id_1));

  // Unmount the original mount.
  ProtoBlob unmount_options_blob = CreateUnmountOptionsBlob(mount_id_1);
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->Unmount(unmount_options_blob)));
  EXPECT_EQ(0, mount_manager_->MountCount());
  EXPECT_FALSE(mount_manager_->IsAlreadyMounted(mount_id_1));

  // Mount a second mount and verify it got a new id.
  int32_t mount_id_2;
  ProtoBlob mount_options_blob_2 = CreateMountOptionsBlob("smb://wdshare/dogs");
  smbprovider_->Mount(mount_options_blob_2, &error, &mount_id_2);
  EXPECT_EQ(1, mount_manager_->MountCount());
  EXPECT_TRUE(mount_manager_->IsAlreadyMounted(mount_id_2));
  EXPECT_NE(mount_id_1, mount_id_2);
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
      999 /* mount_id */, GetAddedFullDirectoryPath());
  smbprovider_->ReadDirectory(read_directory_blob, &err, &results);
  EXPECT_TRUE(results.empty());
  EXPECT_EQ(ERROR_NOT_FOUND, CastError(err));
  ExpectNoOpenEntries();
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

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  ProtoBlob results;
  int32_t err;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, GetDefaultDirectoryPath());
  smbprovider_->ReadDirectory(read_directory_blob, &err, &results);

  DirectoryEntryListProto entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(err));
  EXPECT_EQ(0, entries.entries_size());
  ExpectNoOpenEntries();
}

// Read directory succeeds but does not return files when it exceeds the buffer
// size.
TEST_F(SmbProviderTest, ReadDirectoryDoesNotReturnEntryWithSmallBuffer) {
  // Construct smbprovider_ with buffer size of 1.
  SetSmbProviderBuffer(1);
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());

  const std::string file_path =
      AppendPath(GetAddedFullDirectoryPath(), "file.jpg");

  fake_samba_->AddFile(file_path, kFileSize);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, GetDefaultDirectoryPath());
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryListProto entries;
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

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());

  fake_samba_->AddFile(GetDefaultFullPath("/path/file1.jpg"), kFileSize);
  fake_samba_->AddFile(GetDefaultFullPath("/path/file2.jpg"), kFileSize);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, GetDefaultDirectoryPath());
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryListProto entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(2, entries.entries_size());

  const DirectoryEntryProto& entry1 = entries.entries(0);
  EXPECT_FALSE(entry1.is_directory());
  EXPECT_EQ("file1.jpg", entry1.name());

  const DirectoryEntryProto& entry2 = entries.entries(1);
  EXPECT_FALSE(entry2.is_directory());
  EXPECT_EQ("file2.jpg", entry2.name());
}

// Read directory succeeds and returns expected entries.
TEST_F(SmbProviderTest, ReadDirectorySucceedsWithNonEmptyDir) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());

  fake_samba_->AddFile(GetDefaultFullPath("/path/file.jpg"), kFileSize);
  fake_samba_->AddDirectory(GetDefaultFullPath("/path/images"));

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, GetDefaultDirectoryPath());
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryListProto entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(2, entries.entries_size());

  const DirectoryEntryProto& entry1 = entries.entries(0);
  EXPECT_FALSE(entry1.is_directory());
  EXPECT_EQ("file.jpg", entry1.name());

  const DirectoryEntryProto& entry2 = entries.entries(1);
  EXPECT_TRUE(entry2.is_directory());
  EXPECT_EQ("images", entry2.name());
}

// Read directory succeeds and omits "." and ".." entries.
TEST_F(SmbProviderTest, ReadDirectoryDoesntReturnSelfAndParentEntries) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());

  fake_samba_->AddFile(GetDefaultFullPath("/path/file.jpg"), kFileSize);
  fake_samba_->AddDirectory(GetDefaultFullPath("/path/."));
  fake_samba_->AddDirectory(GetDefaultFullPath("/path/.."));

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, GetDefaultDirectoryPath());
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryListProto entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(1, entries.entries_size());

  const DirectoryEntryProto& entry = entries.entries(0);
  EXPECT_FALSE(entry.is_directory());
  EXPECT_EQ("file.jpg", entry.name());
}

// Read directory fails when called on a file.
TEST_F(SmbProviderTest, ReadDirectoryFailsWithFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath());

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, GetDefaultFilePath());
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  EXPECT_EQ(ERROR_NOT_A_DIRECTORY, CastError(error_code));
}

// Read directory fails when called on a non file.
TEST_F(SmbProviderTest, ReadDirectoryFailsWithNonFileNonDirectory) {
  int32_t mount_id = PrepareMount();
  const std::string printer_path = "/path/canon.cn";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddEntry(GetDefaultFullPath(printer_path), SMBC_PRINTER_SHARE);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, printer_path);
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  EXPECT_EQ(ERROR_NOT_A_DIRECTORY, CastError(error_code));
}

// Read directory succeeds and omits non files / non directories.
TEST_F(SmbProviderTest, ReadDirectoryDoesNotReturnNonFileNonDir) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetDefaultFullPath("/path/file.jpg"));
  fake_samba_->AddEntry(GetDefaultFullPath("/path/canon.cn"),
                        SMBC_PRINTER_SHARE);

  ProtoBlob results;
  int32_t error_code;
  ProtoBlob read_directory_blob =
      CreateReadDirectoryOptionsBlob(mount_id, GetDefaultDirectoryPath());
  smbprovider_->ReadDirectory(read_directory_blob, &error_code, &results);

  DirectoryEntryListProto entries;
  const std::string parsed_proto(results.begin(), results.end());
  EXPECT_TRUE(entries.ParseFromString(parsed_proto));

  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_EQ(1, entries.entries_size());

  const DirectoryEntryProto& entry = entries.entries(0);
  EXPECT_FALSE(entry.is_directory());
  EXPECT_EQ("file.jpg", entry.name());
}

// GetMetadata fails on non files/dirs. Overall, other types
// are treated as if they do not exist.
TEST_F(SmbProviderTest, GetMetadataFailsWithNonFileNonDir) {
  int32_t mount_id = PrepareMount();
  const std::string printer_path = "/path/canon.cn";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddEntry(GetDefaultFullPath(printer_path), SMBC_PRINTER_SHARE);

  ProtoBlob result;
  int32_t error_code;
  ProtoBlob get_metadata_blob =
      CreateGetMetadataOptionsBlob(mount_id, printer_path);
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
  ProtoBlob get_metadata_blob =
      CreateGetMetadataOptionsBlob(123, GetDefaultDirectoryPath());
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

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath(), kFileSize, kFileDate);

  ProtoBlob result;
  int32_t error_code;
  ProtoBlob get_metadata_blob =
      CreateGetMetadataOptionsBlob(mount_id, GetDefaultFilePath());
  smbprovider_->GetMetadataEntry(get_metadata_blob, &error_code, &result);

  DirectoryEntryProto entry;
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

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, GetDefaultFilePath(), false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_NOT_FOUND, error_code);
  EXPECT_EQ(-1, file_id);
}

// OpenFile succeeds and returns a valid file_id when called on a valid file.
TEST_F(SmbProviderTest, OpenFileSucceedsOnValidFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath(), kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, GetDefaultFilePath(), false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_OK, error_code);
  EXPECT_GE(file_id, 0);

  CloseFileHelper(mount_id, file_id);
}

// OpenFile fails when called on a directory.
TEST_F(SmbProviderTest, OpenFileFailsOnDirectory) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, GetDefaultDirectoryPath(), false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_NOT_FOUND, error_code);
  EXPECT_EQ(-1, file_id);
}

// OpenFile fails when called on a non file / non directory.
TEST_F(SmbProviderTest, OpenFileFailsOnNonFileNonDir) {
  int32_t mount_id = PrepareMount();
  const std::string printer_path = "/path/canon.cn";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddEntry(GetDefaultFullPath(printer_path), SMBC_PRINTER_SHARE);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob =
      CreateOpenFileOptionsBlob(mount_id, printer_path, false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  EXPECT_EQ(ERROR_NOT_FOUND, error_code);
  EXPECT_EQ(-1, file_id);
}

// OpenFile sets read and write flags correctly in the corresponding OpenInfo.
TEST_F(SmbProviderTest, OpenFileReadandWriteFlagSetCorrectly) {
  int32_t mount_id = PrepareMount();
  const std::string file_path1 = "/path/dog.jpg";
  const std::string file_path2 = "/path/cat.jpg";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetDefaultFullPath(file_path1), kFileSize, kFileDate);
  fake_samba_->AddFile(GetDefaultFullPath(file_path2), kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob =
      CreateOpenFileOptionsBlob(mount_id, file_path1, false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  int32_t file_id_2;
  int32_t error_code_2;
  open_file_blob =
      CreateOpenFileOptionsBlob(mount_id, file_path2, true /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code_2, &file_id_2);

  EXPECT_EQ(ERROR_OK, error_code);
  EXPECT_TRUE(fake_samba_->HasReadSet(file_id));
  EXPECT_FALSE(fake_samba_->HasWriteSet(file_id));

  EXPECT_EQ(ERROR_OK, error_code_2);
  EXPECT_TRUE(fake_samba_->HasReadSet(file_id_2));
  EXPECT_TRUE(fake_samba_->HasWriteSet(file_id_2));

  CloseFileHelper(mount_id, file_id);
  CloseFileHelper(mount_id, file_id_2);
}

// CloseFile succeeds on a valid file.
TEST_F(SmbProviderTest, CloseFileSucceedsOnOpenFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath(), kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, GetDefaultFilePath(), false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);
  EXPECT_EQ(ERROR_OK, error_code);

  CloseFileHelper(mount_id, file_id);
}

// CloseFile closes the correct file when multiple files are open.
TEST_F(SmbProviderTest, CloseFileClosesCorrectFile) {
  int32_t mount_id = PrepareMount();
  const std::string file_path1 = "/path/dog.jpg";
  const std::string file_path2 = "/path/cat.jpg";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetDefaultFullPath(file_path1), kFileSize, kFileDate);
  fake_samba_->AddFile(GetDefaultFullPath(file_path2), kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob =
      CreateOpenFileOptionsBlob(mount_id, file_path1, false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  int32_t file_id_2;
  int32_t error_code_2;
  open_file_blob =
      CreateOpenFileOptionsBlob(mount_id, file_path2, false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code_2, &file_id_2);

  EXPECT_TRUE(fake_samba_->IsFileFDOpen(file_id));
  EXPECT_FALSE(fake_samba_->IsDirectoryFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFileFDOpen(file_id_2));
  EXPECT_FALSE(fake_samba_->IsDirectoryFDOpen(file_id_2));
  EXPECT_NE(file_id, file_id_2);

  CloseFileHelper(mount_id, file_id);
  EXPECT_FALSE(fake_samba_->IsFileFDOpen(file_id));
  EXPECT_FALSE(fake_samba_->IsDirectoryFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFileFDOpen(file_id_2));
  EXPECT_FALSE(fake_samba_->IsDirectoryFDOpen(file_id_2));

  CloseFileHelper(mount_id, file_id_2);
}

// CloseFile closes the correct instance of a file with opened more than once.
TEST_F(SmbProviderTest, CloseFileClosesCorrectInstanceOfSameFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath(), kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, GetDefaultFilePath(), false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);

  int32_t file_id_2;
  int32_t error_code_2;
  open_file_blob = CreateOpenFileOptionsBlob(mount_id, GetDefaultFilePath(),
                                             false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code_2, &file_id_2);

  EXPECT_TRUE(fake_samba_->IsFileFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFileFDOpen(file_id_2));
  EXPECT_NE(file_id, file_id_2);

  CloseFileHelper(mount_id, file_id);
  EXPECT_FALSE(fake_samba_->IsFileFDOpen(file_id));
  EXPECT_TRUE(fake_samba_->IsFileFDOpen(file_id_2));

  CloseFileHelper(mount_id, file_id_2);
}

// CloseFile fails when called on a closed file.
TEST_F(SmbProviderTest, CloseFileFailsWhenFileIsNotOpen) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath(), kFileSize, kFileDate);

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob = CreateOpenFileOptionsBlob(
      mount_id, GetDefaultFilePath(), false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);
  EXPECT_EQ(ERROR_OK, error_code);
  CloseFileHelper(mount_id, file_id);

  ProtoBlob close_file_blob = CreateCloseFileOptionsBlob(mount_id, file_id);
  EXPECT_EQ(ERROR_NOT_FOUND, smbprovider_->CloseFile(close_file_blob));
}

// CloseFile fails when called with a non-existant file handler.
TEST_F(SmbProviderTest, CloseFileFailsOnNonExistantFileHandler) {
  ProtoBlob close_file_blob =
      CreateCloseFileOptionsBlob(1 /* mount_id */, 1564 /* file_id */);

  EXPECT_EQ(ERROR_NOT_FOUND, smbprovider_->CloseFile(close_file_blob));
}

// DeleteEntry succeeds when called without recursive on an empty directory.
TEST_F(SmbProviderTest, DeleteEntrySucceedsOnEmptyDirectory) {
  int32_t mount_id = PrepareMount();
  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());

  ProtoBlob delete_entry_blob = CreateDeleteEntryOptionsBlob(
      mount_id, GetDefaultDirectoryPath(), false /* recursive */);
  EXPECT_EQ(ERROR_OK, smbprovider_->DeleteEntry(delete_entry_blob));
}

// DeleteEntry succeeds when called on a file.
TEST_F(SmbProviderTest, DeleteEntrySucceedsOnFile) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath(), kFileSize, kFileDate);

  ProtoBlob delete_entry_blob = CreateDeleteEntryOptionsBlob(
      mount_id, GetDefaultFilePath(), false /* recursive */);
  EXPECT_EQ(ERROR_OK, smbprovider_->DeleteEntry(delete_entry_blob));
}

// DeleteEntry fails when called without recursive on a non-empty directory.
TEST_F(SmbProviderTest, DeleteEntryFailsWithoutRecursiveOnNonEmptyDirectory) {
  int32_t mount_id = PrepareMount();

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetAddedFullFilePath(), kFileSize, kFileDate);

  ProtoBlob delete_entry_blob = CreateDeleteEntryOptionsBlob(
      mount_id, GetDefaultDirectoryPath(), false /* recursive */);
  EXPECT_EQ(ERROR_NOT_EMPTY, smbprovider_->DeleteEntry(delete_entry_blob));
}

// DeleteEntry fails when called on non-existent file or directory.
TEST_F(SmbProviderTest, DeleteEntryFailsOnNonExistentEntries) {
  int32_t mount_id = PrepareMount();

  ProtoBlob delete_entry_blob = CreateDeleteEntryOptionsBlob(
      mount_id, GetDefaultDirectoryPath(), false /* recursive */);
  EXPECT_EQ(ERROR_NOT_FOUND, smbprovider_->DeleteEntry(delete_entry_blob));

  ProtoBlob delete_entry_blob_2 =
      CreateDeleteEntryOptionsBlob(mount_id, "/cat.png", false /* recursive */);
  EXPECT_EQ(ERROR_NOT_FOUND, smbprovider_->DeleteEntry(delete_entry_blob_2));
}

// DeleteEntry deletes the correct file.
TEST_F(SmbProviderTest, DeleteEntryDeletesCorrectFile) {
  int32_t mount_id = PrepareMount();
  const std::string file_path1 = "/path/dog.jpg";
  const std::string file_path2 = "/path/cat.jpg";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddFile(GetDefaultFullPath(file_path1), kFileSize, kFileDate);
  fake_samba_->AddFile(GetDefaultFullPath(file_path2), kFileSize, kFileDate);

  ProtoBlob delete_entry_blob = CreateDeleteEntryOptionsBlob(
      mount_id, GetDefaultFilePath(), false /* recursive */);
  EXPECT_EQ(ERROR_OK, smbprovider_->DeleteEntry(delete_entry_blob));

  EXPECT_FALSE(fake_samba_->EntryExists(GetDefaultFullPath(file_path1)));
  EXPECT_TRUE(fake_samba_->EntryExists(GetDefaultFullPath(file_path2)));
}

// DeleteEntry deletes the correct directory.
TEST_F(SmbProviderTest, DeleteEntryDeletesCorrectDirectory) {
  int32_t mount_id = PrepareMount();
  const std::string dir_path1 = "/path/dogs";
  const std::string dir_path2 = "/path/cats";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddDirectory(GetDefaultFullPath(dir_path1));
  fake_samba_->AddDirectory(GetDefaultFullPath(dir_path2));

  ProtoBlob delete_entry_blob =
      CreateDeleteEntryOptionsBlob(mount_id, dir_path1, false /* recursive */);
  EXPECT_EQ(ERROR_OK, smbprovider_->DeleteEntry(delete_entry_blob));

  EXPECT_FALSE(fake_samba_->EntryExists(GetDefaultFullPath(dir_path1)));
  EXPECT_TRUE(fake_samba_->EntryExists(GetDefaultFullPath(dir_path2)));
}

// DeleteEntry should fail on a non-file, non-directory.
TEST_F(SmbProviderTest, DeleteEntryFailsOnNonFileNonDirectory) {
  int32_t mount_id = PrepareMount();
  const std::string printer_path = "/path/canon.cn";

  fake_samba_->AddDirectory(GetAddedFullDirectoryPath());
  fake_samba_->AddEntry(GetDefaultFullPath(printer_path), SMBC_PRINTER_SHARE);

  ProtoBlob delete_entry_blob = CreateDeleteEntryOptionsBlob(
      mount_id, printer_path, false /* recursive */);
  EXPECT_EQ(ERROR_NOT_FOUND, smbprovider_->DeleteEntry(delete_entry_blob));
}

// ReadFile fails when passed in invalid proto.
TEST_F(SmbProviderTest, ReadFileFailsWithInvalidProto) {
  int32_t err;
  ProtoBlob empty_blob;
  dbus::FileDescriptor fd;

  smbprovider_->ReadFile(empty_blob, &err, &fd);

  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED, CastError(err));
  EXPECT_FALSE(fd.is_valid());
}

// ReadFile fails when passed an invalid file descriptor.
TEST_F(SmbProviderTest, ReadFileFailsWithBadFD) {
  int32_t err;
  ProtoBlob blob = CreateReadFileOptionsBlob(0 /* mount_id */, -1 /* file_id */,
                                             0 /* offset */, 1 /* length */);
  dbus::FileDescriptor fd;
  smbprovider_->ReadFile(blob, &err, &fd);

  EXPECT_NE(ERROR_OK, CastError(err));
  EXPECT_FALSE(fd.is_valid());
}

// ReadFile fails when passed an unopened file descriptor.
TEST_F(SmbProviderTest, ReadFileFailsWithUnopenedFD) {
  int32_t err;
  ProtoBlob blob = CreateReadFileOptionsBlob(
      0 /* mount_id */, 100 /* file_id */, 0 /* offset */, 1 /* length */);
  dbus::FileDescriptor fd;
  smbprovider_->ReadFile(blob, &err, &fd);

  EXPECT_NE(ERROR_OK, CastError(err));
  EXPECT_FALSE(fd.is_valid());
}

// ReadFile fails when passed a negative offset.
TEST_F(SmbProviderTest, ReadFileFailsWithNegativeOffset) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int32_t file_id = OpenAddedFile(mount_id);

  int32_t err;
  ProtoBlob blob = CreateReadFileOptionsBlob(mount_id, file_id, -1 /* offset */,
                                             1 /* length */);
  dbus::FileDescriptor fd;
  smbprovider_->ReadFile(blob, &err, &fd);

  EXPECT_NE(ERROR_OK, CastError(err));
  EXPECT_FALSE(fd.is_valid());
}

// ReadFile fails when passed a negative length.
TEST_F(SmbProviderTest, ReadFileFailsWithNegativeLength) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int32_t file_id = OpenAddedFile(mount_id);

  int32_t err;
  ProtoBlob blob = CreateReadFileOptionsBlob(mount_id, file_id, 0 /* offset */,
                                             -1 /* length */);
  dbus::FileDescriptor fd;
  smbprovider_->ReadFile(blob, &err, &fd);

  EXPECT_NE(ERROR_OK, CastError(err));
  EXPECT_FALSE(fd.is_valid());
}

// ReadFile returns a valid file descriptor on success.
TEST_F(SmbProviderTest, ReadFileReturnsValidFileDescriptor) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int32_t file_id = OpenAddedFile(mount_id);

  dbus::FileDescriptor fd;
  ReadFile(mount_id, file_id, 0 /* offset */, file_data.size(), &fd);

  EXPECT_TRUE(fd.is_valid());
  EXPECT_GE(fd.value(), 0);
  CloseFileHelper(mount_id, file_id);
}

// ReadFile should properly call Seek and ending offset for file should be
// (offset + length).
TEST_F(SmbProviderTest, ReadFileSeeksToOffset) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int32_t file_id = OpenAddedFile(mount_id);

  const int64_t offset = 5;
  const int32_t length_to_read = 2;
  DCHECK_GT(file_data.size(), offset);
  DCHECK_GE(file_data.size(), offset + length_to_read);

  EXPECT_EQ(0, fake_samba_->GetFileOffset(file_id));

  dbus::FileDescriptor fd;
  ReadFile(mount_id, file_id, offset, length_to_read, &fd);

  EXPECT_EQ(offset + length_to_read, fake_samba_->GetFileOffset(file_id));
  CloseFileHelper(mount_id, file_id);
}

// ReadFile should properly write the read bytes into a temporary file.
TEST_F(SmbProviderTest, ReadFileWritesTemporaryFile) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int32_t file_id = OpenAddedFile(mount_id);

  const int64_t offset = 3;
  const int32_t length_to_read = 2;

  dbus::FileDescriptor fd;
  ReadFile(mount_id, file_id, offset, length_to_read, &fd);

  // Compare the written value to the expected value.
  ValidateFDContent(fd, length_to_read, file_data.begin() + offset,
                    file_data.begin() + offset + length_to_read);
  CloseFileHelper(mount_id, file_id);
}

// ReadFile should properly read the correct file when there are multiple
// files.
TEST_F(SmbProviderTest, ReadFileReadsCorrectFile) {
  const std::vector<uint8_t> file_data1 = {0, 1, 2, 3, 4, 5};
  const std::vector<uint8_t> file_data2 = {10, 11, 12, 13, 14, 15};
  const std::string file_path = "/path/cat.jpg";

  // PrepareSingleFileMountWithData() prepares a mount and adds a file in
  // GetDefaultFilePath().
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data1);

  // Add an additional file with different data.
  fake_samba_->AddFile(GetDefaultFullPath(file_path), kFileDate, file_data2);

  // Open both files.
  const int32_t file_id1 = OpenAddedFile(mount_id, GetDefaultFilePath());
  const int32_t file_id2 = OpenAddedFile(mount_id, file_path);
  EXPECT_NE(file_id1, file_id2);

  dbus::FileDescriptor fd1;
  ReadFile(mount_id, file_id1, 0 /* offset */, file_data1.size(), &fd1);

  dbus::FileDescriptor fd2;
  ReadFile(mount_id, file_id2, 0 /* offset */, file_data2.size(), &fd2);

  // Compare the written values to the expected values.
  ValidateFDContent(fd1, file_data1.size(), file_data1.begin(),
                    file_data1.end());

  ValidateFDContent(fd2, file_data2.size(), file_data2.begin(),
                    file_data2.end());

  // Close files.
  CloseFileHelper(mount_id, file_id1);
  CloseFileHelper(mount_id, file_id2);
}

// CreateFile fails when passed an invalid protobuf.
TEST_F(SmbProviderTest, CreateFileFailsWithInvalidProto) {
  ProtoBlob empty_blob;
  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED,
            CastError(smbprovider_->CreateFile(empty_blob)));
}

// CreateFile fails when passed an invalid mount.
TEST_F(SmbProviderTest, CreateFileFailsWithInvalidMount) {
  ProtoBlob create_blob =
      CreateCreateFileOptionsBlob(999, GetDefaultFilePath());

  EXPECT_EQ(ERROR_NOT_FOUND, CastError(smbprovider_->CreateFile(create_blob)));
}

// CreateFile succeeds when passed valid parameters and closes the file handle.
TEST_F(SmbProviderTest, CreateFileSucceeds) {
  const int32_t mount_id = PrepareMount();
  const std::string path = "/dog.jpg";

  ProtoBlob create_blob = CreateCreateFileOptionsBlob(mount_id, path);

  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->CreateFile(create_blob)));
  EXPECT_TRUE(fake_samba_->EntryExists(GetDefaultFullPath(path)));
  ExpectNoOpenEntries();
}

// Created file should be able to be opened.
TEST_F(SmbProviderTest, CreatedFileCanBeOpened) {
  const int32_t mount_id = PrepareMount();
  const std::string path = "/dog.jpg";

  ProtoBlob create_blob = CreateCreateFileOptionsBlob(mount_id, path);
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->CreateFile(create_blob)));

  int32_t file_id;
  int32_t error_code;
  ProtoBlob open_file_blob =
      CreateOpenFileOptionsBlob(mount_id, path, false /* writeable */);
  smbprovider_->OpenFile(open_file_blob, &error_code, &file_id);
  EXPECT_EQ(ERROR_OK, CastError(error_code));
  EXPECT_GE(file_id, 0);

  CloseFileHelper(mount_id, file_id);
}

// CreateFile should be able to create multiple files with different paths.
TEST_F(SmbProviderTest, CreateMultipleFiles) {
  const int32_t mount_id = PrepareMount();
  const std::string path1 = "/dog.jpg";
  const std::string path2 = "/cat.jpg";

  ProtoBlob create_blob1 = CreateCreateFileOptionsBlob(mount_id, path1);
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->CreateFile(create_blob1)));
  EXPECT_TRUE(fake_samba_->EntryExists(GetDefaultFullPath(path1)));

  ProtoBlob create_blob2 = CreateCreateFileOptionsBlob(mount_id, path2);
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->CreateFile(create_blob2)));
  EXPECT_TRUE(fake_samba_->EntryExists(GetDefaultFullPath(path2)));
}

// CreateFile should fail if a file already exists in the path.
TEST_F(SmbProviderTest, CreateFileFailsFileAlreadyExists) {
  const int32_t mount_id = PrepareMount();
  const std::string path = "/dog.jpg";

  fake_samba_->AddFile(GetDefaultFullPath("/dog.jpg"));

  ProtoBlob create_blob2 = CreateCreateFileOptionsBlob(mount_id, path);
  EXPECT_EQ(ERROR_EXISTS, CastError(smbprovider_->CreateFile(create_blob2)));
}

// CreateFile should fail if a directory already exists in the path.
TEST_F(SmbProviderTest, CreateFileFailsDirectoryExists) {
  const int32_t mount_id = PrepareMount();
  const std::string directory_path = "/dogs";

  // Add a directory located at "/dogs".
  fake_samba_->AddDirectory(GetDefaultFullPath(directory_path));

  // Attempt to add a file located at "/dogs".
  ProtoBlob create_blob = CreateCreateFileOptionsBlob(mount_id, directory_path);

  EXPECT_EQ(ERROR_EXISTS, CastError(smbprovider_->CreateFile(create_blob)));
}

// Truncate should fail when passed an invalid protobuf.
TEST_F(SmbProviderTest, TruncateFailsWithInvalidProto) {
  ProtoBlob empty_blob;
  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED,
            CastError(smbprovider_->Truncate(empty_blob)));
}

// Truncate should fail when passed a mount id that does not exist.
TEST_F(SmbProviderTest, TruncateFailsWithMountThatDoesntExist) {
  ProtoBlob blob =
      CreateTruncateOptionsBlob(999, GetDefaultFilePath(), 0 /* length */);

  EXPECT_EQ(ERROR_NOT_FOUND, CastError(smbprovider_->Truncate(blob)));
}

// Truncate should fail when passed a file path that does not exist.
TEST_F(SmbProviderTest, TruncateFailsWithFilePathThatDoesntExist) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);

  ProtoBlob blob =
      CreateTruncateOptionsBlob(mount_id, "/foo/bar.txt", 0 /* length */);

  EXPECT_EQ(ERROR_NOT_FOUND, CastError(smbprovider_->Truncate(blob)));
}

// Truncate should fail when passed a negative length.
TEST_F(SmbProviderTest, TruncateFailsWithNegativeLength) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);

  ProtoBlob blob = CreateTruncateOptionsBlob(mount_id, GetDefaultFilePath(),
                                             -1 /* length */);

  EXPECT_EQ(ERROR_DBUS_PARSE_FAILED, CastError(smbprovider_->Truncate(blob)));
}

// Truncate should close the file when truncate returns an error.
TEST_F(SmbProviderTest, TruncateReturnsCorrectError) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int32_t expected_error = EACCES;

  fake_samba_->SetTruncateError(expected_error);

  ProtoBlob blob =
      CreateTruncateOptionsBlob(mount_id, GetDefaultFilePath(), 1 /* length */);

  EXPECT_EQ(GetErrorFromErrno(expected_error),
            CastError(smbprovider_->Truncate(blob)));
  ExpectNoOpenEntries();
}

// Truncate should return the error from truncate even if CloseFile fails.
TEST_F(SmbProviderTest, TruncateReturnsCorrectErrorWhenCloseFileFails) {
  const std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int32_t truncate_error = EACCES;

  // Set the errors that Truncate and Close will return.
  fake_samba_->SetTruncateError(truncate_error);
  fake_samba_->SetCloseFileError(EMFILE);

  // Call Truncate.
  ProtoBlob blob =
      CreateTruncateOptionsBlob(mount_id, GetDefaultFilePath(), 1 /* length */);

  // Error returned should be the one that Truncate returned.
  EXPECT_EQ(GetErrorFromErrno(truncate_error),
            CastError(smbprovider_->Truncate(blob)));
}

// Truncate should successfully change the file size to a smaller length.
TEST_F(SmbProviderTest, TruncateChangesFileSizeToSmallerLength) {
  std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int64_t new_length = 5;

  // Truncate the length to the smaller size.
  ProtoBlob blob =
      CreateTruncateOptionsBlob(mount_id, GetDefaultFilePath(), new_length);

  // Truncate should be successful.
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->Truncate(blob)));

  // Resize the original vector to get the expected value.
  file_data.resize(new_length, 0);
  EXPECT_TRUE(fake_samba_->IsFileDataEqual(GetAddedFullFilePath(), file_data));
  ExpectNoOpenEntries();
}

// Truncate should successfully change the file size to a larger length.
TEST_F(SmbProviderTest, TruncateChangesFileSizeToLargerLength) {
  std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);
  const int64_t new_length = 50;

  // Truncate the length to the larger size.
  ProtoBlob blob =
      CreateTruncateOptionsBlob(mount_id, GetDefaultFilePath(), new_length);

  // Truncate should be successful.
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->Truncate(blob)));

  // Resize the original vector to get the expected value. The appended values
  // should be initialized to '0'.
  file_data.resize(new_length, 0);
  EXPECT_TRUE(fake_samba_->IsFileDataEqual(GetAddedFullFilePath(), file_data));
  ExpectNoOpenEntries();
}

TEST_F(SmbProviderTest, TruncateSucceedsWithSameLength) {
  std::vector<uint8_t> file_data = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const int32_t mount_id = PrepareSingleFileMountWithData(file_data);

  // Truncate the length to the same size.
  ProtoBlob blob = CreateTruncateOptionsBlob(mount_id, GetDefaultFilePath(),
                                             file_data.size());

  // Truncate should be successful.
  EXPECT_EQ(ERROR_OK, CastError(smbprovider_->Truncate(blob)));
  EXPECT_TRUE(fake_samba_->IsFileDataEqual(GetAddedFullFilePath(), file_data));
  ExpectNoOpenEntries();
}

}  // namespace smbprovider
