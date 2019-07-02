// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/biod_storage.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <testing/gtest/include/gtest/gtest.h>

namespace biod {

namespace {

const char kBiometricsManagerName[] = "BiometricsManager";

const char kRecordId1[] = "00000000_0000_0000_0000_000000000001";
const char kUserId1[] = "0000000000000000000000000000000000000001";
const char kLabel1[] = "record1";
const std::vector<uint8_t> kValidationVal1 = {0x00, 0x01};
const char kData1[] = "Hello, world1!";

const char kRecordId2[] = "00000000_0000_0000_0000_000000000002";
const char kUserId2[] = "0000000000000000000000000000000000000002";
const char kLabel2[] = "record2";
const std::vector<uint8_t> kValidationVal2 = {0x00, 0x02};
const char kData2[] = "Hello, world2!";

const char kRecordId3[] = "00000000_0000_0000_0000_000000000003";
const char kLabel3[] = "record3";
const std::vector<uint8_t> kValidationVal3 = {0x00, 0x03};
const char kData3[] = "Hello, world3!";

class TestRecord : public BiometricsManager::Record {
 public:
  TestRecord(const std::string& id,
             const std::string& user_id,
             const std::string& label,
             const std::vector<uint8_t>& validation_val,
             const std::string& data)
      : id_(id),
        user_id_(user_id),
        label_(label),
        validation_val_(validation_val),
        data_(data) {}

  // BiometricsManager::Record overrides:
  const std::string& GetId() const override { return id_; }
  const std::string& GetUserId() const override { return user_id_; }
  const std::string& GetLabel() const override { return label_; }
  const std::vector<uint8_t>& GetValidationVal() const override {
    return validation_val_;
  }
  const std::string& GetData() const { return data_; }
  bool SetLabel(std::string label) override { return true; }
  bool Remove() override { return true; }

  friend bool operator==(const TestRecord& lhs, const TestRecord& rhs) {
    return lhs.id_ == rhs.id_ && lhs.user_id_ == rhs.user_id_ &&
           lhs.validation_val_ == rhs.validation_val_ &&
           lhs.label_ == rhs.label_ && lhs.data_ == rhs.data_;
  }
  friend inline bool operator!=(const TestRecord& lhs, const TestRecord& rhs) {
    return !(lhs == rhs);
  }

 private:
  std::string id_;
  std::string user_id_;
  std::string label_;
  std::vector<uint8_t> validation_val_;
  std::string data_;
};
}  // namespace

class BiodStorageTest : public ::testing::Test {
 public:
  BiodStorageTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    root_path_ = temp_dir_.GetPath().AppendASCII("biod_storage_unittest_root");
    biod_storage_.reset(new BiodStorage(
        kBiometricsManagerName,
        base::Bind(&BiodStorageTest::LoadRecord, base::Unretained(this))));
    // Since there is no session manager, allow accesses by default.
    biod_storage_->set_allow_access(true);
    biod_storage_->SetRootPathForTesting(root_path_);
  }

  ~BiodStorageTest() override {
    EXPECT_TRUE(base::DeleteFile(temp_dir_.GetPath(), true));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath root_path_;
  std::unique_ptr<BiodStorage> biod_storage_;
  std::vector<TestRecord> records_;

 private:
  // LoadRecord is a callback passed to biod_storage_. It gets called when
  // biod_storage_ reads a record from storage. It loads the new record into
  // records_.
  bool LoadRecord(const std::string& user_id,
                  const std::string& label,
                  const std::string& record_id,
                  const std::vector<uint8_t>& validation_val,
                  const base::Value& data_value) {
    std::string data;
    data_value.GetAsString(&data);
    records_.push_back(
        TestRecord(record_id, user_id, label, validation_val, data));
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(BiodStorageTest);
};

TEST_F(BiodStorageTest, WriteAndReadRecords) {
  const std::vector<TestRecord> kRecords = {
      TestRecord(kRecordId1, kUserId1, kLabel1, kValidationVal1, kData1),
      TestRecord(kRecordId2, kUserId2, kLabel2, kValidationVal2, kData2),
      TestRecord(kRecordId3, kUserId2, kLabel3, kValidationVal3, kData3)};

  // Write the record.
  for (auto const& record : kRecords) {
    EXPECT_TRUE(biod_storage_->WriteRecord(
        record, std::make_unique<base::Value>(record.GetData())));
  }

  // Read the record.
  std::unordered_set<std::string> user_ids({kUserId1, kUserId2});
  EXPECT_TRUE(biod_storage_->ReadRecords(user_ids));
  EXPECT_TRUE(
      std::is_permutation(kRecords.begin(), kRecords.end(), records_.begin()));
}

TEST_F(BiodStorageTest, DeleteRecord) {
  const TestRecord kRecord(kRecordId1, kUserId1, kLabel1, kValidationVal1,
                           kData1);

  // Delete a non-existent record.
  EXPECT_TRUE(biod_storage_->DeleteRecord(kUserId1, kRecordId1));

  EXPECT_TRUE(biod_storage_->WriteRecord(
      kRecord, std::make_unique<base::Value>(kRecord.GetData())));

  // Check this record is properly written.
  std::unordered_set<std::string> user_ids({kUserId1});
  EXPECT_TRUE(biod_storage_->ReadRecords(user_ids));
  ASSERT_FALSE(records_.empty());
  ASSERT_EQ(1u, records_.size());
  EXPECT_EQ(records_[0], kRecord);

  records_.clear();

  EXPECT_TRUE(biod_storage_->DeleteRecord(kUserId1, kRecordId1));

  // Check this record is properly deleted.
  EXPECT_TRUE(biod_storage_->ReadRecords(user_ids));
  EXPECT_TRUE(records_.empty());
}

TEST_F(BiodStorageTest, GenerateNewRecordId) {
  // Check the two record ids are different.
  std::string record_id1(biod_storage_->GenerateNewRecordId());
  std::string record_id2(biod_storage_->GenerateNewRecordId());
  EXPECT_NE(record_id1, record_id2);
}

TEST_F(BiodStorageTest, TestEqualOperator) {
  EXPECT_EQ(TestRecord(kRecordId1, kUserId1, kLabel1, kValidationVal1, kData1),
            TestRecord(kRecordId1, kUserId1, kLabel1, kValidationVal1, kData1));

  EXPECT_NE(TestRecord(kRecordId1, kUserId1, kLabel1, kValidationVal1, kData1),
            TestRecord(kRecordId1, kUserId1, kLabel1, kValidationVal2, kData1));
}
}  // namespace biod
