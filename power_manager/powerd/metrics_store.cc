// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <base/eintr_wrapper.h>
#include <base/logging.h>

#include "power_manager/powerd/metrics_store.h"

namespace power_manager {

const char kMetricsStorePath[] = "/var/log/power_manager/powerd-metrics-store";
const int kSizeOfStoredMetrics =
    MetricsStore::kNumOfStoredMetrics * sizeof(int);
const int kReadWriteFlags = S_IRUSR | S_IWUSR;

MetricsStore::MetricsStore()
    : store_fd_(-1),
      store_map_(NULL) {
}

MetricsStore::~MetricsStore() {
  CloseStore();
}

bool MetricsStore::Init() {
  if ((store_fd_ != -1) || (store_map_ != NULL)) {
    LOG(ERROR) << "Store not in default state, not running intialization!";
    return false;
  }

  if (!StoreFileConfigured(kMetricsStorePath))
    if (!(ConfigureStore(kMetricsStorePath))) {
      unlink(kMetricsStorePath);
      return false;
    }

  if (!OpenStoreFile(kMetricsStorePath, &store_fd_, false)) {
    unlink(kMetricsStorePath);
    return false;
  }

  if (!MapStore()) {
    ScrubStore();
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

bool MetricsStore::IsInitialized() const {
  return ((store_fd_ > -1) &&
          (store_map_ != NULL) &&
          (store_map_ != MAP_FAILED));
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
    ScrubStore();
    return false;
  }

  if (HANDLE_EINTR(ftruncate(fd, kSizeOfStoredMetrics)) < 0) {
    LOG(ERROR) << "Failed to truncate/expand file " << file_path
               << " with errno=" << strerror(errno);
    ScrubStore();
    return false;
  }

  close(fd);
  fd = -1;
  return true;
}

bool MetricsStore::OpenStoreFile(const char* file_path,
                                 int* fd,
                                 bool truncate) {
  if (*fd > -1) {
    LOG(ERROR) << "Supplied file descriptor is already set, cannot open store"
               << " file";
    ScrubStore();
    return false;
  }

  int flags = O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW;
  *fd = HANDLE_EINTR(open(file_path,
                          flags,
                          kReadWriteFlags));
  if (*fd > -1)
    return true;
  if (errno != EEXIST) {
    PLOG(ERROR) << "Failed to open persistent metrics store file on first try";
    ScrubStore();
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
  if (*fd > -1)
    return true;
  if (errno != -ELOOP) {
    PLOG(ERROR) << "Failed to open persistent metrics store file second try";
    ScrubStore();
    return false;
  }

  unlink(file_path);

  *fd = HANDLE_EINTR(open(file_path,
                          flags,
                          kReadWriteFlags));
  if (*fd > -1) {
    return true;
  } else {
    LOG(ERROR) << "Failed to open persistent metrics store file third try";
    ScrubStore();
    return false;
  }
}

bool MetricsStore::MapStore() {
  if ((store_fd_ < 0) || (store_map_ != NULL)) {
    LOG(ERROR) << "MetricsStore in incorrect state to map store!";
    ScrubStore();
    return false;
  }

  store_map_ = static_cast<int*>(mmap(NULL,
                                      kSizeOfStoredMetrics,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED,
                                      store_fd_,
                                      0));
  if (store_map_ == MAP_FAILED) {
    LOG(ERROR) << "Failed to mmap file with errno=" << strerror(errno);
    ScrubStore();
    return false;
  }

  return true;
}

bool MetricsStore::SyncStore(int* map) {
  if (map == NULL) {
    LOG(ERROR) << "Tried to sync NULL map!";
    ScrubStore();
    return false;
  }

  if (HANDLE_EINTR(msync(map, kSizeOfStoredMetrics, MS_SYNC)) < 0) {
    LOG(ERROR) << "Failed to msync with errno=" << strerror(errno);
    ScrubStore();
    return false;
  }
  return true;
}

void MetricsStore::CloseStore() {
  if (!IsInitialized()) {
    LOG(ERROR) << "CloseStore called with invalid values.";
    ScrubStore();
    return;
  }


  if (HANDLE_EINTR(munmap(store_map_, kSizeOfStoredMetrics)) < 0) {
    LOG(ERROR) << "Failed to unmap store metrics with errno="
               << strerror(errno);
  }
  close(store_fd_);

  store_map_ = NULL;
  store_fd_ = -1;
}

void MetricsStore::ResetMetric(const StoredMetric& metric) {
  if (!IsInitialized()) {
    LOG(WARNING) << "Attempted to reset metric when not initialized";
    return;
  }

  if ((metric < 0) || (metric >= kNumOfStoredMetrics)) {
    LOG(WARNING) << "ResetMetric: Metric index out of range, metric = "
                 << metric;
    return;
  }

  SetMetric(metric, 0);
}

void MetricsStore::IncrementMetric(const StoredMetric& metric) {
  if (!IsInitialized()) {
    LOG(WARNING) << "Attempted to increment metric when not initialized";
    return;
  }

  if ((metric < 0) || (metric >= kNumOfStoredMetrics)) {
    LOG(WARNING) << "IncrementMetric: Metric index out of range, metric = "
                 << metric;
    return;
  }

  LOG(INFO) << "Current value in store map is " << store_map_[metric];
  LOG(INFO) << "Current value of store map is " << store_map_;

  store_map_[metric]++;
  SyncStore(store_map_);
}

void MetricsStore::SetMetric(const StoredMetric& metric, const int& value) {
  if (!IsInitialized()) {
    LOG(WARNING) << "Attempted to set metric when not initialized";
    return;
  }

  if ((metric < 0) || (metric >= kNumOfStoredMetrics)) {
    LOG(WARNING) << "SetMetric: Metric index out of range, metric = " << metric;
    return;
  }

  store_map_[metric] = value;
  SyncStore(store_map_);
}

int MetricsStore::GetMetric(const StoredMetric& metric) {
  if (!IsInitialized()) {
    LOG(WARNING) << "Attempted to get metric when not initialized";
    return -1;
  }

  if ((metric < 0) || (metric >= kNumOfStoredMetrics)) {
    LOG(WARNING) << "GetMetric: Metric index out of range, metric = " << metric;
    return -1;
  }

  return store_map_[metric];
}

void MetricsStore::ScrubStore() {
  LOG(ERROR) << "Metrics store has gotten into a bad state, so we are unmapping"
             << " it and removing the backing file";

  if ((store_map_ != NULL) && (store_map_ != MAP_FAILED)) {
    if (HANDLE_EINTR(munmap(store_map_, kSizeOfStoredMetrics)) < 0) {
      LOG(ERROR) << "Failed to unmap store metrics with errno="
                 << strerror(errno);
    }
  }

  if (store_fd_ > -1)
    close(store_fd_);

  store_map_ = NULL;
  store_fd_ = -1;

  unlink(kMetricsStorePath);
}


};  // namespace power_manager
