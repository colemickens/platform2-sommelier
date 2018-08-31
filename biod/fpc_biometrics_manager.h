// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_FPC_BIOMETRICS_MANAGER_H_
#define BIOD_FPC_BIOMETRICS_MANAGER_H_

#include "biod/biometrics_manager.h"

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/synchronization/lock.h>
#include <base/threading/thread.h>
#include <base/values.h>

#include "biod/bio_library.h"
#include "biod/biod_storage.h"

namespace biod {

class FpcBiometricsManager : public BiometricsManager {
 public:
  static std::unique_ptr<BiometricsManager> Create();
  // The current fp_pal API requires direct operations on the sensor without
  // passing the context back to the caller, so we need to make the context
  // accessible globally.
  // Make the FD accessible to the PAL. There's only one sensor, opened on biod
  // startup and closed on exit, so the FD is const after Init().
  static int g_sensor_fd;
  static const int kIRQTimeoutMs = 10000;

  // BiometricsManager overrides:
  ~FpcBiometricsManager() override;

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
  // Keep this function here because it's used directly by the End* functions.
  void KillSensorTask();

  using TaskRunnerRef = scoped_refptr<base::SingleThreadTaskRunner>;

  class SensorLibrary;

  struct ScanData {
    struct Killed {};
    ScanData() = default;
    explicit ScanData(const Killed&) : killed(true) {}
    explicit ScanData(ScanResult r) : success(true), result(r) {}
    explicit ScanData(BioImage i) : success(true), image(std::move(i)) {}

    // True if the scan ended because of kill_task_
    bool killed = false;
    // True if there were NO systemic errors
    bool success = false;
    // Meaningless if success is false, kSuccess on a good scan, user
    // correctable error otherwise
    ScanResult result = ScanResult::SCAN_RESULT_SUCCESS;
    // If success and result is kSuccess, this contains the scanned image
    BioImage image;

    explicit operator bool() {
      return !killed && success && result == ScanResult::SCAN_RESULT_SUCCESS &&
             image;
    }
  };

  struct InternalRecord {
    std::string user_id;
    std::string label;
    BioTemplate tmpl;
  };

  // Our Record implementation is just a proxy for InternalRecord, which are all
  // stored inside the FakeBiometricsManager object's records map.
  class Record : public BiometricsManager::Record {
   public:
    Record(const base::WeakPtr<FpcBiometricsManager>& biometrics_manager,
           std::string id)
        : biometrics_manager_(biometrics_manager), id_(id) {}

    // BiometricsManager::Record overrides:
    const std::string& GetId() const override;
    const std::string& GetUserId() const override;
    const std::string& GetLabel() const override;
    bool SetLabel(std::string label) override;
    bool Remove() override;

   private:
    base::WeakPtr<FpcBiometricsManager> biometrics_manager_;
    std::string id_;

    // These are mutable because the const GetUserId and GetLabel methods use
    // them as storage for the their respective return string refs.
    mutable std::string local_user_id_;
    mutable std::string local_label_;

    // Only call while holding the records_lock_
    InternalRecord* GetInternalLocked() const;

    using RecordIterator =
        std::unordered_map<std::string, InternalRecord>::iterator;
    // Used to ensure that the internal record is always handled with proper
    // locks and checking.
    template <typename F>
    bool WithInternal(F f) const {
      if (!biometrics_manager_)
        return false;
      base::AutoLock guard(biometrics_manager_->records_lock_);
      auto internal_record = biometrics_manager_->records_.find(id_);
      if (internal_record == biometrics_manager_->records_.end())
        return false;
      f(internal_record);
      return true;
    }
  };

  FpcBiometricsManager();
  bool Init();

  // These are basic wrappers for the client callback functions. We use them for
  // two reasons:
  // - these will always work even if the underlying callbacks are null
  // - the address of these methods never changes, which is important when
  //   posting tasks onto the main thread from the sensor thread.
  void OnEnrollScanDone(ScanResult result,
                        const BiometricsManager::EnrollStatus& enroll_status);
  void OnAuthScanDone(ScanResult result,
                      const BiometricsManager::AttemptMatches& matches);
  void OnSessionFailed();

  // This function are sensor thread only.
  ScanData ScanImage();

  // The following Do*Task and On*Complete functions are meant to be run as a
  // pair using TaskRunner::PostTaskAndReply. The Do*Task functions run in the
  // sensor thread and only read sensor_data_, the dynamically loaded functions,
  // kill_task_, and the sensor itself. The On*Complete functions interpret the
  // result of the task function on the main thread and change the state of this
  // BiometricsManager and its records as needed.

  void DoEnrollSessionTask(const TaskRunnerRef& task_runner,
                           const std::shared_ptr<BioTemplate>& tmpl);
  void OnEnrollSessionComplete(std::string user_id,
                               std::string label,
                               const std::shared_ptr<BioTemplate>& tmpl);

  void DoAuthSessionTask(
      const TaskRunnerRef& task_runner,
      std::shared_ptr<std::unordered_set<std::string>> updated_record_ids);
  void OnAuthSessionComplete(
      std::shared_ptr<std::unordered_set<std::string>> updated_record_ids);

  void OnTaskComplete();

  bool LoadRecord(const std::string& user_id,
                  const std::string& label,
                  const std::string& record_id,
                  const base::Value& data);
  bool WriteRecord(const BiometricsManager::Record& record,
                   uint8_t* tmpl_data,
                   size_t tmpl_size);

  // The following variables are const after Init and therefore totally thread
  // safe.

  base::ScopedFD sensor_fd_;

  // Only used by the sensor thread after Init.
  std::shared_ptr<BioLibrary> bio_lib_;
  std::unique_ptr<SensorLibrary> sensor_lib_;

  // Variables used to control the sensor thread and are shared.
  bool running_task_ = false;
  base::Lock kill_task_lock_;
  bool kill_task_ = false;
  base::Thread sensor_thread_;

  // This lock protects records_.
  base::Lock records_lock_;
  std::unordered_map<std::string, InternalRecord> records_;

  // All the following variables are main thread only.

  BiometricsManager::EnrollScanDoneCallback on_enroll_scan_done_;
  BiometricsManager::AuthScanDoneCallback on_auth_scan_done_;
  BiometricsManager::SessionFailedCallback on_session_failed_;

  base::WeakPtrFactory<FpcBiometricsManager> session_weak_factory_;
  base::WeakPtrFactory<FpcBiometricsManager> weak_factory_;

  BiodStorage biod_storage_;

  DISALLOW_COPY_AND_ASSIGN(FpcBiometricsManager);
};
}  // namespace biod

#endif  // BIOD_FPC_BIOMETRICS_MANAGER_H_
