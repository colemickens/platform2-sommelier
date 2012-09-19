// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <base/eintr_wrapper.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "power_manager/powerd/metrics_store.h"

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
    CreateFakeMap();
    CreateTestFile();
    CreateTestSymLink();
  }

  virtual void TearDown() {
    DestroyFakeMap();
    DestroyTestFile();
    DestroyTestSymLink();
  }

  virtual void CreateFakeMap() {
    fake_store_ = static_cast<int*>(mmap(NULL,
                                         kSizeOfStoredMetrics,
                                         PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANON,
                                         -1,
                                         0));
    metrics_store_.store_map_ = fake_store_;
    metrics_store_.store_fd_ = kTestFD;
  }


  virtual void DestroyFakeMap() {
    metrics_store_.store_map_ = NULL;
    metrics_store_.store_fd_ = -1;
    HANDLE_EINTR(munmap(fake_store_, kSizeOfStoredMetrics));
  }

  virtual void CreateRealMap() {
    metrics_store_.store_fd_ = HANDLE_EINTR(open(kTestFileName,
                                                 O_RDWR | O_CREAT | O_TRUNC,
                                                 kReadWriteFlags));
    HANDLE_EINTR(ftruncate(metrics_store_.store_fd_, kSizeOfStoredMetrics));

    metrics_store_.store_map_ = static_cast<int*>(mmap(NULL,
                                                       kSizeOfStoredMetrics,
                                                       PROT_READ | PROT_WRITE,
                                                       MAP_PRIVATE | MAP_ANON,
                                                       metrics_store_.store_fd_,
                                                       0));
  }

  virtual void DestroyRealMap() {
    HANDLE_EINTR(munmap(metrics_store_.store_map_, kSizeOfStoredMetrics));
    if (metrics_store_.store_fd_ > -1)
      close(metrics_store_.store_fd_);
    metrics_store_.store_map_ = NULL;
    metrics_store_.store_fd_ = -1;
  }

  virtual void CreateTestFile() {
    // Creating the file for the test
    int fd = HANDLE_EINTR(open(kTestFileName,
                               O_RDWR | O_CREAT | O_TRUNC,
                               kReadWriteFlags));
    // Sizing the test file
    HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics));
    close(fd);
  }

  virtual void CreateTestFileWrongSize() {
    // Creating the file for the test
    int fd = HANDLE_EINTR(open(kTestFileName,
                               O_RDWR | O_CREAT | O_TRUNC,
                               kReadWriteFlags));
    // Sizing the test file
    HANDLE_EINTR(ftruncate(fd, 0));
    close(fd);
  }

  virtual void DestroyTestFile() {
    HANDLE_EINTR(remove(kTestFileName));
  }

  virtual void CreateTestSymLink() {
    if (HANDLE_EINTR(symlink(kTestFileName, kTestSymLinkName)) < 0) {
      LOG(ERROR) << "Failed to create symlink" << kTestSymLinkName << " of "
                 << kTestFileName << " with errno=" << strerror(errno);
    }
  }

  virtual void DestroyTestSymLink() {
    HANDLE_EINTR(remove(kTestSymLinkName));
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
  // Expecting to return true when the file exists and is sized
  EXPECT_TRUE(metrics_store_.StoreFileConfigured(kTestFileName));
}

TEST_F(MetricsStoreTest, StoreFileConfiguredNoFile) {
  // Expecting to return false when the file dne
  DestroyTestFile();
  EXPECT_FALSE(metrics_store_.StoreFileConfigured(kTestFileName));
}

TEST_F(MetricsStoreTest, StoreFileConfiguredWrongSize) {
  // Expecting to return false since the file is the wrong size
  DestroyTestFile();
  CreateTestFileWrongSize();
  EXPECT_FALSE(metrics_store_.StoreFileConfigured(kTestFileName));
}

TEST_F(MetricsStoreTest, StoreFileConfiguredSymLink) {
  // Make sure that we die when the file is a sym link
  EXPECT_FALSE(metrics_store_.StoreFileConfigured(kTestSymLinkName));
}


TEST_F(MetricsStoreTest, ConfigureStoreNoFile) {
  // Expecting file to be created when it dne
  DestroyTestFile();
  EXPECT_TRUE(metrics_store_.ConfigureStore(kTestFileName));
  struct stat st;
  EXPECT_EQ(0, lstat(kTestFileName, &st));
  EXPECT_EQ(MetricsStore::kNumOfStoredMetrics * sizeof(int), st.st_size);
}

TEST_F(MetricsStoreTest, ConfigureStoreFileExists) {
  // Expecting file to be opened fine when it exists already
  EXPECT_TRUE(metrics_store_.ConfigureStore(kTestFileName));
  struct stat st;
  EXPECT_EQ(0, lstat(kTestFileName, &st));
  EXPECT_EQ(MetricsStore::kNumOfStoredMetrics * sizeof(int), st.st_size);
}

TEST_F(MetricsStoreTest, OpenStoreFileNoFile) {
  int fd = -1;
  DestroyTestFile();
  EXPECT_TRUE(metrics_store_.OpenStoreFile(kTestFileName, &fd, false));
  EXPECT_NE(fd, -1);
}

TEST_F(MetricsStoreTest, OpenStoreFileExists) {
  int fd = -1;
  EXPECT_TRUE(metrics_store_.OpenStoreFile(kTestFileName, &fd, false));
  EXPECT_GT(fd, -1);
}

TEST_F(MetricsStoreTest, OpenStoreFileTruncate) {
  DestroyTestFile();
  CreateTestFileWrongSize();
  int fd = -1;
  EXPECT_TRUE(metrics_store_.OpenStoreFile(kTestFileName, &fd, true));
  EXPECT_GT(fd, -1);
}

TEST_F(MetricsStoreTest, OpenStoreFileFDSet) {
  int fd = kTestFD;
  EXPECT_FALSE(metrics_store_.OpenStoreFile(kTestFileName, &fd, false));
  EXPECT_TRUE(metrics_store_.store_map_ == NULL);
  EXPECT_EQ(metrics_store_.store_fd_, -1);
}

TEST_F(MetricsStoreTest, OpenStoreFileSymLink) {
  int fd = -1;
  EXPECT_FALSE(metrics_store_.OpenStoreFile(kTestSymLinkName, &fd, false));
  EXPECT_EQ(fd, -1);
}

TEST_F(MetricsStoreTest, MapStoreSuccess) {
  DestroyFakeMap();
  metrics_store_.store_fd_ = HANDLE_EINTR(open(kTestFileName,
                                               O_RDWR | O_CREAT | O_TRUNC,
                                               kReadWriteFlags));
  HANDLE_EINTR(ftruncate(metrics_store_.store_fd_ , kSizeOfStoredMetrics));
  // We should be able to map to this file and location with no issues
  EXPECT_TRUE(metrics_store_.MapStore());
  EXPECT_TRUE(metrics_store_.store_map_ != NULL);
  DestroyRealMap();
}

TEST_F(MetricsStoreTest, MapStoreBadFD) {
  DestroyFakeMap();
  EXPECT_FALSE(metrics_store_.MapStore());
  EXPECT_TRUE(metrics_store_.store_map_ == NULL);
  EXPECT_EQ(metrics_store_.store_fd_, -1);
}

TEST_F(MetricsStoreTest, MapStoreAlreadyMapped) {
  EXPECT_FALSE(metrics_store_.MapStore());
  EXPECT_TRUE(metrics_store_.store_map_ == NULL);
  EXPECT_EQ(metrics_store_.store_fd_, -1);
}

TEST_F(MetricsStoreTest, MapStoreMapCallFails) {
  DestroyFakeMap();
  metrics_store_.store_fd_ = kTestFD;
  EXPECT_FALSE(metrics_store_.MapStore());
  EXPECT_TRUE(metrics_store_.store_map_ == NULL);
  EXPECT_EQ(metrics_store_.store_fd_, -1);
}

TEST_F(MetricsStoreTest, SyncStoreSuccess) {
  DestroyFakeMap();
  CreateRealMap();
  metrics_store_.SyncStore(metrics_store_.store_map_);
  DestroyRealMap();
}

TEST_F(MetricsStoreTest, SyncStoreNullMap) {
  DestroyFakeMap();
  EXPECT_FALSE(metrics_store_.SyncStore(NULL));
  EXPECT_TRUE(metrics_store_.store_map_ == NULL);
  EXPECT_EQ(metrics_store_.store_fd_, -1);
}

TEST_F(MetricsStoreTest, CloseStoreSuccess) {
  DestroyFakeMap();
  CreateRealMap();
  metrics_store_.CloseStore();
  EXPECT_EQ(-1, metrics_store_.store_fd_);
  EXPECT_TRUE(metrics_store_.store_map_ == NULL);
}

// This method should silently handle bad inputs since it is called by the
// destructor
TEST_F(MetricsStoreTest, CloseStoreBadFD) {
  int fd = metrics_store_.store_fd_;
  metrics_store_.store_fd_ = -1;
  metrics_store_.CloseStore();
  metrics_store_.store_fd_ = fd;
  DestroyFakeMap();
}

TEST_F(MetricsStoreTest, CloseStoreBadMap) {
  DestroyFakeMap();
  metrics_store_.store_fd_ = kTestFD;
  metrics_store_.CloseStore();
}

TEST_F(MetricsStoreTest, ResetMetricSuccess) {
  fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric] = kTestMetricValue;
  metrics_store_.ResetMetric(MetricsStore::kNumOfSessionsPerChargeMetric);
  EXPECT_EQ(0, fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, ResetMetricUnderflow) {
  metrics_store_.ResetMetric(
      static_cast<MetricsStore::StoredMetric>(-1));
}

TEST_F(MetricsStoreTest, ResetMetricOverflow) {
  metrics_store_.ResetMetric(MetricsStore::kNumOfStoredMetrics);
}

TEST_F(MetricsStoreTest, IncrementMetricSuccess) {
  metrics_store_.IncrementMetric(MetricsStore::kNumOfSessionsPerChargeMetric);
  EXPECT_EQ(1, fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, IncrementMetricUnderflow) {
  metrics_store_.IncrementMetric(
      static_cast<MetricsStore::StoredMetric>(-1));
}

TEST_F(MetricsStoreTest, IncrementMetricOverflow) {
  metrics_store_.IncrementMetric(MetricsStore::kNumOfStoredMetrics);
}

TEST_F(MetricsStoreTest, SetMetricSuccess) {
  metrics_store_.SetMetric(MetricsStore::kNumOfSessionsPerChargeMetric,
                           kTestMetricValue);
  EXPECT_EQ(kTestMetricValue,
            fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric]);
}

TEST_F(MetricsStoreTest, SetMetricUnderflow) {
  metrics_store_.SetMetric(static_cast<MetricsStore::StoredMetric>(-1),
                           kTestMetricValue);
}

TEST_F(MetricsStoreTest, SetMetricOverflow) {
  metrics_store_.SetMetric(MetricsStore::kNumOfStoredMetrics,
                           kTestMetricValue);
}

TEST_F(MetricsStoreTest, GetMetricSuccess) {
  fake_store_[MetricsStore::kNumOfSessionsPerChargeMetric] = kTestMetricValue;
  EXPECT_EQ(kTestMetricValue,
            metrics_store_.GetMetric(
                MetricsStore::kNumOfSessionsPerChargeMetric));
}

TEST_F(MetricsStoreTest, GetMetricUnderflow) {
  EXPECT_EQ(
      metrics_store_.GetMetric(static_cast<MetricsStore::StoredMetric>(-1)),
      -1);
}

TEST_F(MetricsStoreTest, GetMetricOverflow) {
  EXPECT_EQ(
      metrics_store_.GetMetric(static_cast<MetricsStore::StoredMetric>(
          MetricsStore::kNumOfStoredMetrics)),
      -1);
}

};  // namespace power_manager
