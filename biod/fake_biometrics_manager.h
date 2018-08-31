// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_FAKE_BIOMETRICS_MANAGER_H_
#define BIOD_FAKE_BIOMETRICS_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>

#include "biod/biod_storage.h"
#include "biod/biometrics_manager.h"
#include "biod/fake_biometrics_manager_common.h"

namespace biod {

class FakeBiometricsManager : public BiometricsManager,
                              public base::MessageLoopForIO::Watcher {
 public:
  FakeBiometricsManager();

  // base::MessageLoopForIO::Watcher overrides:
  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  // BiometricsManager overrides:
  BiometricType GetType() override;
  BiometricsManager::EnrollSession StartEnrollSession(
      std::string user_id, std::string label) override;
  BiometricsManager::AuthSession StartAuthSession() override;
  std::vector<std::unique_ptr<BiometricsManager::Record>> GetRecords() override;
  bool DestroyAllRecords() override;
  void RemoveRecordsFromMemory() override;
  bool ReadRecords(const std::unordered_set<std::string>& user_ids) override;
  bool ReadRecordsForSingleUser(const std::string& user_id) override;

  void SetEnrollScanDoneHandler(const BiometricsManager::EnrollScanDoneCallback&
                                    on_enroll_scan_done) override;
  void SetAuthScanDoneHandler(const BiometricsManager::AuthScanDoneCallback&
                                  on_auth_scan_done) override;
  void SetSessionFailedHandler(const BiometricsManager::SessionFailedCallback&
                                   on_session_failed) override;

 protected:
  void EndEnrollSession() override;
  void EndAuthSession() override;

 private:
  // This structure stores the record data internally to this class, and is
  // exposed with our Record implementation.
  struct InternalRecord {
    std::string user_id;
    std::string label;
  };

  // Our Record implementation is just a proxy for InternalRecord, which
  // are all stored inside the FakeBiometricsManager object's records map.
  class Record : public BiometricsManager::Record {
   public:
    Record(const base::WeakPtr<FakeBiometricsManager>& biometrics_manager,
           std::string id)
        : biometrics_manager_(biometrics_manager), id_(id) {}

    // BiometricsManager::Record overrides:
    const std::string& GetId() const override;
    const std::string& GetUserId() const override;
    const std::string& GetLabel() const override;
    bool SetLabel(std::string label) override;
    bool Remove() override;

   private:
    base::WeakPtr<FakeBiometricsManager> biometrics_manager_;
    std::string id_;

    InternalRecord* GetInternal() const;
  };

  enum class Mode {
    kNone,
    kEnrollSession,
    kAuthSession,
  };

  Mode mode_ = Mode::kNone;

  InternalRecord next_internal_record_;
  std::unordered_map<std::string, InternalRecord> records_;

  base::ScopedFD fake_input_;
  std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  BiometricsManager::EnrollScanDoneCallback on_enroll_scan_done_;
  BiometricsManager::AuthScanDoneCallback on_auth_scan_done_;
  BiometricsManager::SessionFailedCallback on_session_failed_;

  base::WeakPtrFactory<FakeBiometricsManager> session_weak_factory_;
  base::WeakPtrFactory<FakeBiometricsManager> weak_factory_;

  BiodStorage biod_storage_;

  bool LoadRecord(const std::string& user_id,
                  const std::string& label,
                  const std::string& record_id,
                  const base::Value& data);

  DISALLOW_COPY_AND_ASSIGN(FakeBiometricsManager);
};
}  // namespace biod

#endif  // BIOD_FAKE_BIOMETRICS_MANAGER_H_
