// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_METRICS_STORE_H_
#define POWER_MANAGER_METRICS_STORE_H_

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace power_manager {

extern const char kMetricsStorePath[];
extern const int kSizeOfStoredMetrics;
extern const int kReadWriteFlags;

class MetricsStore {
 public:
  enum StoredMetric {
    kNumOfSessionsPerChargeMetric = 0,
    kNumOfStoredMetrics
  };

  MetricsStore();
  virtual ~MetricsStore();
  virtual bool Init();

  // NumSessionsPerCharge Methods
  // These methods will eat calls to them if the store is not mapped
  virtual void ResetNumOfSessionsPerChargeMetric();
  virtual void IncrementNumOfSessionsPerChargeMetric();
  virtual int GetNumOfSessionsPerChargeMetric();

  // Returns whether or not the MetricsStore has been initialized
  // correctly
  virtual bool IsInitialized() const;

 private:
  friend class MetricsStoreTest;
  FRIEND_TEST(MetricsStoreTest, ResetNumOfSessionsPerChargeMetric);
  FRIEND_TEST(MetricsStoreTest, IncrementNumOfSessionsPerChargeMetric);
  FRIEND_TEST(MetricsStoreTest, GetNumOfSessionsPerChargeMetric);
  FRIEND_TEST(MetricsStoreTest, StoreFileConfigured);
  FRIEND_TEST(MetricsStoreTest, StoreFileConfiguredSuccess);
  FRIEND_TEST(MetricsStoreTest, StoreFileConfiguredNoFile);
  FRIEND_TEST(MetricsStoreTest, StoreFileConfiguredWrongSize);
  FRIEND_TEST(MetricsStoreTest, StoreFileConfiguredSymLink);
  FRIEND_TEST(MetricsStoreTest, ConfigureStoreNoFile);
  FRIEND_TEST(MetricsStoreTest, ConfigureStoreFileExists);
  FRIEND_TEST(MetricsStoreTest, OpenStoreFileNoFile);
  FRIEND_TEST(MetricsStoreTest, OpenStoreFileExists);
  FRIEND_TEST(MetricsStoreTest, OpenStoreFileTruncate);
  FRIEND_TEST(MetricsStoreTest, OpenStoreFileFDSet);
  FRIEND_TEST(MetricsStoreTest, OpenStoreFileSymLink);
  FRIEND_TEST(MetricsStoreTest, MapStoreSuccess);
  FRIEND_TEST(MetricsStoreTest, MapStoreBadFD);
  FRIEND_TEST(MetricsStoreTest, MapStoreAlreadyMapped);
  FRIEND_TEST(MetricsStoreTest, MapStoreMapCallFails);
  FRIEND_TEST(MetricsStoreTest, SyncStoreSuccess);
  FRIEND_TEST(MetricsStoreTest, SyncStoreNullMap);
  FRIEND_TEST(MetricsStoreTest, CloseStoreSuccess);
  FRIEND_TEST(MetricsStoreTest, CloseStoreBadFD);
  FRIEND_TEST(MetricsStoreTest, CloseStoreBadMap);
  FRIEND_TEST(MetricsStoreTest, ResetMetricSuccess);
  FRIEND_TEST(MetricsStoreTest, ResetMetricUnderflow);
  FRIEND_TEST(MetricsStoreTest, ResetMetricOverflow);
  FRIEND_TEST(MetricsStoreTest, IncrementMetricSuccess);
  FRIEND_TEST(MetricsStoreTest, IncrementMetricUnderflow);
  FRIEND_TEST(MetricsStoreTest, IncrementMetricOverflow);
  FRIEND_TEST(MetricsStoreTest, SetMetricSuccess);
  FRIEND_TEST(MetricsStoreTest, SetMetricUnderflow);
  FRIEND_TEST(MetricsStoreTest, SetMetricOverflow);
  FRIEND_TEST(MetricsStoreTest, GetMetricSuccess);
  FRIEND_TEST(MetricsStoreTest, GetMetricUnderflow);
  FRIEND_TEST(MetricsStoreTest, GetMetricOverflow);

  // Initializer Utility Functions
  bool StoreFileConfigured(const char* file_path);
  bool ConfigureStore(const char* file_path);
  bool OpenStoreFile(const char* file_path, int* fd, bool truncate);
  bool MapStore();
  bool SyncStore(int* map);

  // Destructor Utility Functions
  void CloseStore();

  // Accessor Utility Functions
  void ResetMetric(const StoredMetric& metric);
  void IncrementMetric(const StoredMetric& metric);
  void SetMetric(const StoredMetric& metric, const int& value);
  int GetMetric(const StoredMetric& metric);

  // Utility function to clean up the MetricsStore when we get into a
  // bad state. This method unmaps the store, closes the file, and
  // removes it.
  void ScrubStore();

  int store_fd_;
  int* store_map_;
  DISALLOW_COPY_AND_ASSIGN(MetricsStore);
};  // class MetricsStore

}  // namespace power_manager
#endif  // POWER_MANAGER_METRICS_STORE_H_
