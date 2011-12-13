// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <base/eintr_wrapper.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "power_manager/metrics_store.h"

namespace power_manager {

using ::testing::Test;

const int kTestMetricValue = 100;
const int kTestFD = 10;
const char kTestFileName[] = "metrics_store_test_file";
const char kTestSymLinkName[] = "metrics_store_test_symlink";

class MetricsStoreTest : public Test {
 public:
  MetricsStoreTest() {}

  virtual void SetUp() {
    fake_store_ = static_cast<int*>(mmap(NULL,
                                kSizeOfStoredMetrics,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANON,
                                -1,
                                0));
    metrics_store_.store_map_ = fake_store_;
  }

  virtual void TearDown() {
    HANDLE_EINTR(remove(kTestFileName));
    HANDLE_EINTR(remove(kTestSymLinkName));
    metrics_store_.store_map_ = NULL;
    HANDLE_EINTR(munmap(fake_store_, kSizeOfStoredMetrics));
  }

 protected:
  MetricsStore metrics_store_;
  int* fake_store_;
};  // class MetricsStoreTest

// User facing tests, these are more like functional tests then true unittests
TEST_F(MetricsStoreTest, ResetNumOfSessionsPerChargeMetric) {
  fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric] = kTestMetricValue;
  metrics_store_.ResetNumOfSessionsPerChargeMetric();
  EXPECT_EQ(0, fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, IncrementNumOfSessionsPerChargeMetric) {
  metrics_store_.IncrementNumOfSessionsPerChargeMetric();
  EXPECT_EQ(1, fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, GetNumOfSessionsPerChargeMetric) {
  fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric] = kTestMetricValue;
  EXPECT_EQ(kTestMetricValue,
            metrics_store_.GetNumOfSessionsPerChargeMetric());
}


TEST_F(MetricsStoreTest, StoreFileConfiguredSuccess) {
  // Creating the file for the test
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  // Sizing the test file
  HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics));
  close(fd);

  // Expecting to return true when the file exists and is sized
  EXPECT_TRUE(metrics_store_.StoreFileConfigured(kTestFileName));
}

TEST_F(MetricsStoreTest, StoreFileConfiguredNoFile) {
  // Expecting to return false when the file dne
  EXPECT_FALSE(metrics_store_.StoreFileConfigured(kTestFileName));
}

TEST_F(MetricsStoreTest, StoreFileConfiguredWrongSize) {
  // Creating the file for the test
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  // Sizing the test file
  HANDLE_EINTR(ftruncate(fd, 10 * kSizeOfStoredMetrics));
  close(fd);

  // Expecting to return false since the file is the wrong size
  EXPECT_FALSE(metrics_store_.StoreFileConfigured(kTestFileName));
}

TEST_F(MetricsStoreTest, StoreFileConfiguredSymLink) {
  // Creating the file for the test
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  // Sizing the test file
  HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics));
  close(fd);

  // Creating the sym link
  if (HANDLE_EINTR(symlink(kTestFileName, kTestSymLinkName)) < 0) {
    LOG(ERROR) << "Failed to create symlink" << kTestSymLinkName << " of "
               << kTestFileName << " with errno=" << strerror(errno);
  }

  // Make sure that we die when the file is a sym link
  EXPECT_FALSE(metrics_store_.StoreFileConfigured(kTestSymLinkName));
}


TEST_F(MetricsStoreTest, ConfigureStoreNoFile) {
  // Expecting file to be created when it dne
  EXPECT_TRUE(metrics_store_.ConfigureStore(kTestFileName));
  struct stat st;
  EXPECT_EQ( 0, lstat(kTestFileName, &st));
  EXPECT_EQ(MetricsStore::kNumOfStoredMetrics * sizeof(int), st.st_size);
}

TEST_F(MetricsStoreTest, ConfigureStoreFileExists) {
  // Creating the file for the test
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  close(fd);

  // Expecting file to be opened fine when it exists already
  EXPECT_TRUE(metrics_store_.ConfigureStore(kTestFileName));
  struct stat st;
  EXPECT_EQ(0, lstat(kTestFileName, &st));
  EXPECT_EQ(MetricsStore::kNumOfStoredMetrics * sizeof(int), st.st_size);
}

TEST_F(MetricsStoreTest, OpenStoreFileNoFile) {
  int fd = -1;
  EXPECT_TRUE(metrics_store_.OpenStoreFile(kTestFileName, &fd, false));
}

TEST_F(MetricsStoreTest, OpenStoreFileExists) {
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  close(fd);
  fd = -1;
  EXPECT_TRUE(metrics_store_.OpenStoreFile(kTestFileName, &fd, false));
}

TEST_F(MetricsStoreTest, OpenStoreFileTruncate) {
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  close(fd);
  fd = -1;
  EXPECT_TRUE(metrics_store_.OpenStoreFile(kTestFileName, &fd, true));
}

TEST_F(MetricsStoreTest, OpenStoreFileFDSet) {
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  close(fd);
  fd = kTestFD;
  EXPECT_DEATH(metrics_store_.OpenStoreFile(kTestFileName, &fd, false), ".*");
}

TEST_F(MetricsStoreTest, OpenStoreFileSymLink) {
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  close(fd);
  fd = -1;
  if (HANDLE_EINTR(symlink(kTestFileName, kTestSymLinkName)) < 0) {
    LOG(ERROR) << "Failed to create symlink" << kTestSymLinkName << " of "
               << kTestFileName << " with errno=" << strerror(errno);
  }
  EXPECT_FALSE(metrics_store_.OpenStoreFile(kTestSymLinkName, &fd, false));
}

TEST_F(MetricsStoreTest, MapStoreSuccess) {
  // Need to setup a file such that the class can mmap it
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));
  HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics));
  metrics_store_.store_map_ = NULL;

  // We should be able to map to this file and location with no issues
  EXPECT_TRUE(metrics_store_.MapStore(fd, &(metrics_store_.store_map_)));
  EXPECT_TRUE(metrics_store_.store_map_ != NULL);

  HANDLE_EINTR(munmap(metrics_store_.store_map_, kSizeOfStoredMetrics));
  close(fd);
}

TEST_F(MetricsStoreTest, MapStoreBadFD) {
  EXPECT_DEATH(metrics_store_.MapStore(-1, &(metrics_store_.store_map_)), ".*");
}

TEST_F(MetricsStoreTest, MapStoreSetStore) {
  metrics_store_.store_map_ = fake_store_;
  EXPECT_DEATH(metrics_store_.MapStore(kTestFD,
                                       &(metrics_store_.store_map_)),
               ".*");
}

TEST_F(MetricsStoreTest, MapStoreMapFails) {
  metrics_store_.store_map_ = NULL;
  EXPECT_FALSE(metrics_store_.MapStore(kTestFD,
                                       &(metrics_store_.store_map_)));
}

TEST_F(MetricsStoreTest, SyncStoreSuccess) {
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));

  HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics));
  metrics_store_.store_map_ = static_cast<int*>(
      mmap(0,
           kSizeOfStoredMetrics,
           PROT_READ | PROT_WRITE,
           MAP_PRIVATE,
           fd,
           0));
  metrics_store_.SyncStore(metrics_store_.store_map_);
  HANDLE_EINTR(munmap(metrics_store_.store_map_, kSizeOfStoredMetrics));
  close(fd);
}

TEST_F(MetricsStoreTest, SyncStoreNullMap) {
  EXPECT_DEATH(metrics_store_.SyncStore(NULL), ".*");
}

TEST_F(MetricsStoreTest, SyncStoreInvalidMap) {
  int invalid_map[kSizeOfStoredMetrics];
  EXPECT_FALSE(metrics_store_.SyncStore(invalid_map));
}

TEST_F(MetricsStoreTest, CloseStoreSuccess) {
  // Setting up a mmap to be torn down
  int fd = HANDLE_EINTR(open(kTestFileName,
                             O_RDWR | O_CREAT | O_TRUNC,
                             kReadWriteFlags));

  HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics));
  metrics_store_.store_map_ = static_cast<int*>(
      mmap(0,
           kSizeOfStoredMetrics,
           PROT_READ | PROT_WRITE,
           MAP_PRIVATE,
           fd,
           0));

  metrics_store_.CloseStore(&fd, &metrics_store_.store_map_);
  EXPECT_EQ(-1, fd);
  EXPECT_TRUE(metrics_store_.store_map_ == NULL);

}

// This method should silently handle bad inputs since it is called by the
// destructor
TEST_F(MetricsStoreTest, CloseStoreBadFD) {
  int fd = -1;
  metrics_store_.CloseStore(&fd, &metrics_store_.store_map_);
}

TEST_F(MetricsStoreTest, CloseStoreBadMap) {
  int fd = kTestFD;
  metrics_store_.store_map_ = NULL;
  metrics_store_.CloseStore(&fd, &metrics_store_.store_map_);
}

TEST_F(MetricsStoreTest, ResetMetricSuccess) {
  fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric] = kTestMetricValue;
  metrics_store_.ResetMetric(MetricsStore::kNumOfSessionsPerChargeMetric);
  EXPECT_EQ(0, fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, ResetMetricUnderflow) {
  EXPECT_DEATH(metrics_store_.ResetMetric(
      static_cast<MetricsStore::StoredMetric>(-1)), ".*");
}

TEST_F(MetricsStoreTest, ResetMetricOverflow) {
  EXPECT_DEATH(metrics_store_.ResetMetric(MetricsStore::kNumOfStoredMetrics),
               ".*");
}

TEST_F(MetricsStoreTest, IncrementMetricSuccess) {
  metrics_store_.IncrementMetric(MetricsStore::kNumOfSessionsPerChargeMetric);
  EXPECT_EQ(1, fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, IncrementMetricUnderflow) {
  EXPECT_DEATH(metrics_store_.IncrementMetric(
      static_cast<MetricsStore::StoredMetric>(-1)), ".*");
}

TEST_F(MetricsStoreTest, IncrementMetricOverflow) {
  EXPECT_DEATH(metrics_store_.IncrementMetric(
      MetricsStore::kNumOfStoredMetrics), ".*");
}

TEST_F(MetricsStoreTest, SetMetricSuccess) {
  metrics_store_.SetMetric(MetricsStore::kNumOfSessionsPerChargeMetric,
                           kTestMetricValue);
  EXPECT_EQ(kTestMetricValue,
            fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, SetMetricUnderflow) {
  EXPECT_DEATH(metrics_store_.SetMetric(
      static_cast<MetricsStore::StoredMetric>(-1), kTestMetricValue),
               ".*");
}

TEST_F(MetricsStoreTest, SetMetricOverflow) {
  EXPECT_DEATH(metrics_store_.SetMetric(MetricsStore::kNumOfStoredMetrics,
                                        kTestMetricValue), ".*");
}

TEST_F(MetricsStoreTest, GetMetricSuccess) {
  fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric] = kTestMetricValue;
  EXPECT_EQ(kTestMetricValue,
            metrics_store_.GetMetric(
                MetricsStore::kNumOfSessionsPerChargeMetric));
}

TEST_F(MetricsStoreTest, GetMetricUnderflow) {
  EXPECT_DEATH(metrics_store_.GetMetric(
      static_cast<MetricsStore::StoredMetric>(-1)), ".*");
}

TEST_F(MetricsStoreTest, GetMetricOverflow) {
  EXPECT_DEATH(metrics_store_.GetMetric(MetricsStore::kNumOfStoredMetrics),
               ".*");
}

};  // namespace power_manager
