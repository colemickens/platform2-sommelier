// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>

#include "smbprovider/constants.h"
#include "smbprovider/proto.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/smbprovider_helper.h"
#include "smbprovider/smbprovider_test_helper.h"

namespace smbprovider {

class SmbProviderProtoTest : public testing::Test {
 public:
  SmbProviderProtoTest() = default;
  ~SmbProviderProtoTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbProviderProtoTest);
};

// Blobs should be serialized correctly.
TEST_F(SmbProviderProtoTest, SerializeProtoToBlob) {
  DirectoryEntryProto entry;
  entry.set_is_directory(true);
  entry.set_name("test");
  entry.set_size(0);
  entry.set_last_modified_time(0);

  ProtoBlob blob;

  EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(entry, &blob));
}

// IsValidOptions returns true when options are valid for valid protos.
TEST_F(SmbProviderProtoTest, IsValidOptionsForValidProtos) {
  MountOptionsProto mount_proto = CreateMountOptionsProto("smb://testShare");
  EXPECT_TRUE(IsValidOptions(mount_proto));

  UnmountOptionsProto unmount_proto =
      CreateUnmountOptionsProto(3 /* mount_id */);
  EXPECT_TRUE(IsValidOptions(unmount_proto));

  ReadDirectoryOptionsProto read_directory_proto =
      CreateReadDirectoryOptionsProto(3 /* mount_id */, "smb://testShare/dir1");
  EXPECT_TRUE(IsValidOptions(read_directory_proto));

  GetMetadataEntryOptionsProto get_metadata_proto =
      CreateGetMetadataOptionsProto(3 /* mount_id */,
                                    "smb://testShare/dir1/pic.png");
  EXPECT_TRUE(IsValidOptions(get_metadata_proto));

  OpenFileOptionsProto open_file_proto = CreateOpenFileOptionsProto(
      3 /* mount_id */, "smb://testShare/dir1/pic.png", false /* writeable */);
  EXPECT_TRUE(IsValidOptions(open_file_proto));

  CloseFileOptionsProto close_file_proto =
      CreateCloseFileOptionsProto(3 /* mount_id */, 4 /* file_id */);
  EXPECT_TRUE(IsValidOptions(close_file_proto));

  DeleteEntryOptionsProto delete_entry_proto = CreateDeleteEntryOptionsProto(
      3 /* mount_id */, "smb://testShare/dir1/pic.png", false /* recursive */);
  EXPECT_TRUE(IsValidOptions(delete_entry_proto));

  ReadFileOptionsProto read_file_proto = CreateReadFileOptionsProto(
      3 /* mount_id */, 4 /* file_id */, 0 /* offset */, 0 /* length */);
  EXPECT_TRUE(IsValidOptions(read_file_proto));

  CreateFileOptionsProto create_file_proto = CreateCreateFileOptionsProto(
      3 /* mount_id */, "smb://testShare/dir1/pic.png");
  EXPECT_TRUE(IsValidOptions(create_file_proto));
}

// IsValidOptions returns false when options are invalid for invalid protos.
TEST_F(SmbProviderProtoTest, IsValidOptionsForInValidProtos) {
  MountOptionsProto mount_proto_blank;
  EXPECT_FALSE(IsValidOptions(mount_proto_blank));

  UnmountOptionsProto unmount_proto_blank;
  EXPECT_FALSE(IsValidOptions(unmount_proto_blank));

  ReadDirectoryOptionsProto read_directory_proto_blank;
  EXPECT_FALSE(IsValidOptions(read_directory_proto_blank));

  GetMetadataEntryOptionsProto get_metadata_proto_blank;
  EXPECT_FALSE(IsValidOptions(get_metadata_proto_blank));

  OpenFileOptionsProto open_file_proto_blank;
  EXPECT_FALSE(IsValidOptions(open_file_proto_blank));

  CloseFileOptionsProto close_file_proto_blank;
  EXPECT_FALSE(IsValidOptions(close_file_proto_blank));

  DeleteEntryOptionsProto delete_entry_proto_blank;
  EXPECT_FALSE(IsValidOptions(delete_entry_proto_blank));

  ReadFileOptionsProto read_file_proto_blank;
  EXPECT_FALSE(IsValidOptions(read_file_proto_blank));

  CreateFileOptionsProto create_file_proto_blank;
  EXPECT_FALSE(IsValidOptions(create_file_proto_blank));
}

// IsValidOptions checks offset and length ranges for ReadFileOptionsProto.
TEST_F(SmbProviderProtoTest, IsValidOptionsChecksReadFileRanges) {
  ReadFileOptionsProto read_file_proto = CreateReadFileOptionsProto(
      3 /* mount_id */, 4 /* file_id */, 0 /* offset */, 0 /* length */);
  EXPECT_TRUE(IsValidOptions(read_file_proto));

  ReadFileOptionsProto read_file_proto_bad_offset = CreateReadFileOptionsProto(
      3 /* mount_id */, 4 /* file_id */, -5 /* offset */, 0 /* length */);
  EXPECT_FALSE(IsValidOptions(read_file_proto_bad_offset));

  ReadFileOptionsProto read_file_proto_bad_length = CreateReadFileOptionsProto(
      3 /* mount_id */, 4 /* file_id */, 0 /* offset */, -10 /* length */);
  EXPECT_FALSE(IsValidOptions(read_file_proto_bad_length));
}

}  // namespace smbprovider
