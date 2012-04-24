// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <base/eintr_wrapper.h>
#include <base/logging.h>

#include "power_manager/metrics_store.h"

namespace power_manager {

const char kMetricsStorePath[] = "/var/log/power_manager/powerd-metrics-store";
const int kSizeOfStoredMetrics =
    MetricsStore::kNumOfStoredMetrics * sizeof(int);
const int kReadWriteFlags = S_IRUSR | S_IWUSR;

MetricsStore::MetricsStore()
    : store_fd_(-1),
      store_map_(NULL),
      is_broken_(false) {
}

MetricsStore::~MetricsStore() {
  CloseStore(&store_fd_, &store_map_);
}

bool MetricsStore::Init() {
  CHECK_EQ(store_fd_, -1);
  CHECK(store_map_ == NULL);

  if (!StoreFileConfigured(kMetricsStorePath))
    if (!(ConfigureStore(kMetricsStorePath))) {
      StoreBroke();
      return false;
    }

  if (!OpenStoreFile(kMetricsStorePath, &store_fd_, false)) {
    StoreBroke();
    return false;
  }

  if (!MapStore(store_fd_, &store_map_)) {
    StoreBroke();
    return false;
  }

  return true;
}

void MetricsStore::ResetNumOfSessionsPerChargeMetric() {
  ResetMetric(kNumOfSessionsPerChargeMetric);
}

void MetricsStore::IncrementNumOfSessionsPerChargeMetric() {
  IncrementMetric(kNumOfSessionsPerChargeMetric);
}

int MetricsStore::GetNumOfSessionsPerChargeMetric() {
  return GetMetric(kNumOfSessionsPerChargeMetric);
}

bool MetricsStore::IsBroken() {
  return is_broken_;
}

bool MetricsStore::StoreFileConfigured(const char* file_path) {
  struct stat st;
  if (lstat(file_path, &st) != 0) {
    LOG(INFO) << "Backing file for metrics store does not exist";
    return false;
  }

  if (S_ISLNK(st.st_mode)) {
    LOG(INFO) << "Backing file is a symbolic link, removing it";
    unlink(file_path);
    return false;
  }

  if (st.st_size != kSizeOfStoredMetrics) {
    LOG(INFO) << "Backing file for metrics store is incorrect size, current = "
              << st.st_size << ", expected = " << kSizeOfStoredMetrics;
    return false;
  }
  return true;
}

bool MetricsStore::ConfigureStore(const char* file_path) {
  int fd = -1;

  if (!OpenStoreFile(file_path, &fd, true)) {
    LOG(ERROR) << "Call to OpenStore failed";
    return false;
  }

  if (HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics)) < 0) {
    LOG(ERROR) << "Failed to truncate/expand file " << file_path
               << " with errno=" << strerror(errno);
    StoreBroke();
    return false;
  }

  close(fd);
  fd = -1;
  return true;
}

bool MetricsStore::OpenStoreFile(const char* file_path,
                                 int* fd,
                                 bool truncate) {
  CHECK_EQ(*fd, -1);

  int flags = O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW;
  *fd = HANDLE_EINTR(open(file_path,
                          flags,
                          kReadWriteFlags));
  if (*fd >= 0)
    return true;
  if (errno != EEXIST) {
    PLOG(ERROR) << "Failed to open persistent metrics store file on first try";
    StoreBroke();
    return false;
  }

  if (truncate) {
      unlink(file_path);
  } else {
    flags = O_RDWR | O_NOFOLLOW;
  }

  *fd = HANDLE_EINTR(open(file_path,
                          flags,
                          kReadWriteFlags));
  if (*fd >= 0)
    return true;
  if (errno != -ELOOP) {
    PLOG(ERROR) << "Failed to open persistent metrics store file second try";
    StoreBroke();
    return false;
  }

  unlink(file_path);

  *fd = HANDLE_EINTR(open(file_path,
                          flags,
                          kReadWriteFlags));
  if (*fd >= 0) {
    return true;
  } else {
    LOG(ERROR) << "Failed to open persistent metrics store file second try";
    StoreBroke();
    return false;
  }
}

bool MetricsStore::MapStore(const int fd, int** map) {
  CHECK_GT(fd, -1);
  CHECK(*map == NULL);

  *map = static_cast<int*>(mmap(NULL,
                                kSizeOfStoredMetrics,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd,
                                0));

  if (*map == MAP_FAILED) {
    LOG(ERROR) << "Failed to mmap file with errno=" << strerror(errno);
    StoreBroke();
    return false;
  }
  return true;
}

bool MetricsStore::SyncStore(int* map) {
  CHECK(map);
  if (HANDLE_EINTR(msync(map, kSizeOfStoredMetrics, MS_SYNC)) < 0) {
    LOG(ERROR) << "Failed to msync with errno=" << strerror(errno);
    StoreBroke();
    return false;
  }
  return true;
}

void MetricsStore::CloseStore(int* fd, int** map) {
  if (*fd > -1) {
    if (*map != NULL) {
      if (HANDLE_EINTR(munmap(*map, kSizeOfStoredMetrics)) < 0) {
        LOG(ERROR) << "Failed to unmap store metrics with errno="
                   << strerror(errno);
        StoreBroke();
      }
      *map = NULL;
    }
    close(*fd);
    *fd = -1;
  }
}

void MetricsStore::ResetMetric(const StoredMetric& metric) {
  if (is_broken_)
    return;
  CHECK_GE(metric, 0);
  CHECK_LT(metric, kNumOfStoredMetrics);

  SetMetric(metric, 0);
}

void MetricsStore::IncrementMetric(const StoredMetric& metric) {
  if (is_broken_)
    return;
  CHECK_GE(metric, 0);
  CHECK_LT(metric, kNumOfStoredMetrics);

  LOG(INFO) << "Current value in store map is " << store_map_[metric];
  LOG(INFO) << "Current value of store map is " << store_map_;

  store_map_[metric]++;
  SyncStore(store_map_);
}

void MetricsStore::SetMetric(const StoredMetric& metric, const int& value) {
  if (is_broken_)
    return;
  CHECK_GE(metric, 0);
  CHECK_LT(metric, kNumOfStoredMetrics);

  store_map_[metric] = value;
  SyncStore(store_map_);
}

int MetricsStore::GetMetric(const StoredMetric& metric) {
  CHECK(!is_broken_);
  CHECK_GE(metric, 0);
  CHECK_LT(metric, kNumOfStoredMetrics);

  return store_map_[metric];
}

void MetricsStore::StoreBroke() {
  if (is_broken_)
    return;
  LOG(ERROR) << "Metrics store has gotten into a bad state, so we are flagging"
             << " as broken and removing the backing file";
  unlink(kMetricsStorePath);
  is_broken_ = true;
}

};  // namespace power_manager
