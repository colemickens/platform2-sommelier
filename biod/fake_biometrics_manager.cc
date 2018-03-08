// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/fake_biometrics_manager.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/values.h>

namespace biod {

const std::string& FakeBiometricsManager::Record::GetId() const {
  return id_;
}

const std::string& FakeBiometricsManager::Record::GetUserId() const {
  InternalRecord* internal = GetInternal();
  CHECK(internal)
      << "Attempted to get user ID for invalid BiometricsManager Record";
  return internal->user_id;
}

const std::string& FakeBiometricsManager::Record::GetLabel() const {
  InternalRecord* internal = GetInternal();
  CHECK(internal)
      << "Attempted to get label for invalid BiometricsManager Record";
  return internal->label;
}

bool FakeBiometricsManager::Record::SetLabel(std::string label) {
  InternalRecord* internal = GetInternal();
  if (internal) {
    internal->label = std::move(label);
    // Set label by overwriting records in file.
    return biometrics_manager_->biod_storage_.WriteRecord(
        *this,
        std::make_unique<base::Value>("Hello, world!"));
  }
  LOG(ERROR) << "Attempt to set label for invalid BiometricsManager Record";
  return false;
}

bool FakeBiometricsManager::Record::Remove() {
  // Delete one record.
  if (biometrics_manager_->biod_storage_.DeleteRecord(
          biometrics_manager_->records_[id_].user_id, id_)) {
    return biometrics_manager_->records_.erase(id_) > 0;
  } else {
    return false;
  }
}

FakeBiometricsManager::InternalRecord*
FakeBiometricsManager::Record::GetInternal() const {
  if (!biometrics_manager_)
    return nullptr;
  auto internal_record = biometrics_manager_->records_.find(id_);
  if (internal_record == biometrics_manager_->records_.end())
    return nullptr;
  return &internal_record->second;
}

FakeBiometricsManager::FakeBiometricsManager()
    : session_weak_factory_(this),
      weak_factory_(this),
      biod_storage_(kFakeBiometricsManagerName,
                    base::Bind(&FakeBiometricsManager::LoadRecord,
                               base::Unretained(this))) {
  const char kFakeInputPath[] = "/tmp/fake_biometric";
  base::DeleteFile(base::FilePath(kFakeInputPath), false);
  CHECK_EQ(mkfifo(kFakeInputPath, 0600), 0)
      << "Failed to create FakeBiometricsManager input";
  // The pipe gets opened read/write to avoid triggering a constant stream of
  // POLLHUP after the pipe is opened writable and closed. The pipe is never
  // actually written to here.
  fake_input_ = base::ScopedFD(open(kFakeInputPath, O_RDWR | O_NONBLOCK));
  CHECK_GE(fake_input_.get(), 0)
      << "Failed to open FakeBiometricsManager input";

  fd_watcher_ = std::make_unique<base::MessageLoopForIO::FileDescriptorWatcher>(
      FROM_HERE);

  CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
      fake_input_.get(),
      true,
      base::MessageLoopForIO::WATCH_READ,
      fd_watcher_.get(),
      this))
      << "Failed to watch FakeBiometricsManager input";
}

BiometricType FakeBiometricsManager::GetType() {
  return BIOMETRIC_TYPE_FINGERPRINT;
}

BiometricsManager::EnrollSession FakeBiometricsManager::StartEnrollSession(
    std::string user_id, std::string label) {
  if (mode_ == Mode::kNone) {
    mode_ = Mode::kEnrollSession;
    next_internal_record_ = {std::move(user_id), std::move(label)};
    return BiometricsManager::EnrollSession(session_weak_factory_.GetWeakPtr());
  }
  return BiometricsManager::EnrollSession();
}

BiometricsManager::AuthSession FakeBiometricsManager::StartAuthSession() {
  if (mode_ == Mode::kNone) {
    mode_ = Mode::kAuthSession;
    return BiometricsManager::AuthSession(session_weak_factory_.GetWeakPtr());
  }
  return BiometricsManager::AuthSession();
}

std::vector<std::unique_ptr<BiometricsManager::Record>>
FakeBiometricsManager::GetRecords() {
  std::vector<std::unique_ptr<BiometricsManager::Record>> records(
      records_.size());
  std::transform(records_.begin(),
                 records_.end(),
                 records.begin(),
                 [this](decltype(records_)::value_type& record) {
                   return std::unique_ptr<BiometricsManager::Record>(
                       new Record(weak_factory_.GetWeakPtr(), record.first));
                 });
  return records;
}

bool FakeBiometricsManager::DestroyAllRecords() {
  // Enumerate through records_ and delete each record.
  bool delete_all_records = true;
  for (auto& record_pair : records_) {
    delete_all_records &= biod_storage_.DeleteRecord(record_pair.second.user_id,
                                                     record_pair.first);
  }
  records_.clear();
  return delete_all_records;
}

void FakeBiometricsManager::RemoveRecordsFromMemory() {
  records_.clear();
}

bool FakeBiometricsManager::ReadRecords(
    const std::unordered_set<std::string>& user_ids) {
  return biod_storage_.ReadRecords(user_ids);
}

void FakeBiometricsManager::SetEnrollScanDoneHandler(
    const BiometricsManager::EnrollScanDoneCallback& on_enroll_scan_done) {
  on_enroll_scan_done_ = on_enroll_scan_done;
}

void FakeBiometricsManager::SetAuthScanDoneHandler(
    const BiometricsManager::AuthScanDoneCallback& on_auth_scan_done) {
  on_auth_scan_done_ = on_auth_scan_done;
}

void FakeBiometricsManager::SetSessionFailedHandler(
    const BiometricsManager::SessionFailedCallback& on_session_failed) {
  on_session_failed_ = on_session_failed;
}

void FakeBiometricsManager::EndEnrollSession() {
  CHECK(mode_ == Mode::kEnrollSession);
  session_weak_factory_.InvalidateWeakPtrs();
  mode_ = Mode::kNone;
}

void FakeBiometricsManager::EndAuthSession() {
  CHECK(mode_ == Mode::kAuthSession);
  session_weak_factory_.InvalidateWeakPtrs();
  mode_ = Mode::kNone;
}

void FakeBiometricsManager::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void FakeBiometricsManager::OnFileCanReadWithoutBlocking(int fd) {
  // We scan the stream for the magic bytes in case the previous input command
  // was not the correct length or malformed for whatever reason. This must be
  // done a single byte at a time because the input stream of bytes is totally
  // unaligned. Reading the length of magic bytes at once might consume some
  // garbage data and the start of the magic bytes, but that would fail to
  // validate, and subsequent reads would never see that correct instance of
  // magic bytes.
  size_t magic_index = 0;
  const uint8_t magic_start[] = {FAKE_BIOMETRIC_MAGIC_BYTES};
  while (magic_index < sizeof(magic_start)) {
    uint8_t magic;
    if (read(fd, &magic, sizeof(magic)) != sizeof(magic))
      return;
    if (magic == magic_start[magic_index])
      magic_index++;
    else
      magic_index = 0;
  }

  uint8_t cmd;
  if (read(fd, &cmd, sizeof(cmd)) != sizeof(cmd))
    return;
  switch (cmd) {
    case 'A': {
      uint8_t res_code;
      if (read(fd, &res_code, sizeof(res_code)) != sizeof(res_code))
        return;
      ScanResult res = static_cast<ScanResult>(res_code);

      uint8_t match_user_count;
      if (read(fd, &match_user_count, sizeof(match_user_count)) !=
          sizeof(match_user_count))
        return;

      BiometricsManager::AttemptMatches matches;
      for (uint8_t match_index = 0; match_index < match_user_count;
           match_index++) {
        uint8_t user_id_size;
        if (read(fd, &user_id_size, sizeof(user_id_size)) !=
            sizeof(user_id_size))
          return;

        std::string user_id(user_id_size, '\0');
        if (read(fd, &user_id.front(), user_id.size()) != user_id.size())
          return;

        // These record IDs are interpreted as record identifiers by biod and
        // its clients.
        std::vector<std::string>& record_ids = matches[user_id];

        uint8_t record_id_count;
        if (read(fd, &record_id_count, sizeof(record_id_count)) !=
            sizeof(record_id_count))
          return;

        for (uint8_t record_id_index = 0; record_id_index < record_id_count;
             record_id_index++) {
          uint8_t record_id_size;
          if (read(fd, &record_id_size, sizeof(record_id_size)) !=
              sizeof(record_id_size))
            return;

          std::string record_id(record_id_size, '\0');
          int ret = read(fd, &record_id.front(), record_id.size());
          if (ret != record_id.size()) {
            LOG(ERROR) << "failed to read record id " << errno;
            return;
          }

          record_ids.emplace_back(std::move(record_id));
        }
        std::string record_ids_joined;
        for (const auto& record_id : record_ids) {
          record_ids_joined += " \"" + record_id + "\"";
        }
        LOG(INFO) << "Recognized User " << user_id
                  << " with record ids:" << record_ids_joined;
      }

      if (!on_auth_scan_done_.is_null() && mode_ == Mode::kAuthSession)
        on_auth_scan_done_.Run(res, matches);
      return;
    }
    case 'S': {
      uint8_t res_code;
      if (read(fd, &res_code, sizeof(res_code)) != sizeof(res_code))
        return;
      ScanResult res = static_cast<ScanResult>(res_code);

      uint8_t done;
      if (read(fd, &done, sizeof(done)) != sizeof(done))
        return;

      LOG(INFO) << "Scan result " << static_cast<int>(res_code) << " done "
                << static_cast<bool>(done);
      if (mode_ == Mode::kEnrollSession) {
        if (done) {
          std::string record_id(biod_storage_.GenerateNewRecordId());

          records_[record_id] = std::move(next_internal_record_);
          Record current_record(weak_factory_.GetWeakPtr(), record_id);

          if (!biod_storage_.WriteRecord(
                  current_record,
                  std::make_unique<base::Value>("Hello, world!"))) {
            records_.erase(record_id);
          }
          mode_ = Mode::kNone;
          session_weak_factory_.InvalidateWeakPtrs();
        }

        if (!on_enroll_scan_done_.is_null()) {
          BiometricsManager::EnrollStatus enroll_status = {done > 0, -1};
          on_enroll_scan_done_.Run(res, enroll_status);
        }
      }
      return;
    }
    case 'F':
      LOG(INFO) << "Fake failure";
      if (!on_session_failed_.is_null())
        on_session_failed_.Run();
  }
}

bool FakeBiometricsManager::LoadRecord(const std::string& user_id,
                                       const std::string& label,
                                       const std::string& record_id,
                                       const base::Value& data) {
  InternalRecord internal_record = {user_id, label};
  records_[record_id] = std::move(internal_record);
  LOG(INFO) << "Load record " << record_id << " from disk.";
  return true;
}
}  // namespace biod
