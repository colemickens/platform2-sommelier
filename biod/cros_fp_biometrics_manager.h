// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_CROS_FP_BIOMETRICS_MANAGER_H_
#define BIOD_CROS_FP_BIOMETRICS_MANAGER_H_

#include "biod/biometrics_manager.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <base/values.h>
#include <dbus/bus.h>

#include "biod/biod_storage.h"
#include "biod/cros_fp_device.h"
#include "biod/power_button_filter_interface.h"

namespace biod {

class BiodMetrics;

class CrosFpBiometricsManager : public BiometricsManager {
 public:
  static std::unique_ptr<BiometricsManager> Create(
      const scoped_refptr<dbus::Bus>& bus);

  // BiometricsManager overrides:
  ~CrosFpBiometricsManager() override;

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

  bool SendStatsOnLogin() override;

  void SetDiskAccesses(bool allow) override;

  bool ResetSensor() override;

  bool ResetEntropy(bool factory_init) override;

 protected:
  void EndEnrollSession() override;
  void EndAuthSession() override;

 private:
  using SessionAction = base::Callback<void(const uint32_t event)>;

  struct InternalRecord {
    std::string record_id;
    std::string user_id;
    std::string label;
  };

  class Record : public BiometricsManager::Record {
   public:
    Record(const base::WeakPtr<CrosFpBiometricsManager>& biometrics_manager,
           int index)
        : biometrics_manager_(biometrics_manager), index_(index) {}

    // BiometricsManager::Record overrides:
    const std::string& GetId() const override;
    const std::string& GetUserId() const override;
    const std::string& GetLabel() const override;
    bool SetLabel(std::string label) override;
    bool Remove() override;

   private:
    base::WeakPtr<CrosFpBiometricsManager> biometrics_manager_;
    int index_;

    // These are mutable because the const GetId, GetUserId and GetLabel methods
    // use them as storage for the their respective return string refs.
    mutable std::string local_record_id_;
    mutable std::string local_user_id_;
    mutable std::string local_label_;
  };

  explicit CrosFpBiometricsManager(
      std::unique_ptr<PowerButtonFilterInterface> power_button_filter);
  bool Init();

  void OnEnrollScanDone(ScanResult result,
                        const BiometricsManager::EnrollStatus& enroll_status);
  void OnAuthScanDone(ScanResult result,
                      const BiometricsManager::AttemptMatches& matches);
  void OnSessionFailed();

  void OnMkbpEvent(uint32_t event);

  // Request an action from the Fingerprint MCU and set the appropriate callback
  // when the event with the result will arrive.
  bool RequestEnrollImage(InternalRecord record);
  bool RequestEnrollFingerUp(InternalRecord record);
  bool RequestMatch(int attempt = 0);
  bool RequestMatchFingerUp();

  // Actions taken when the corresponding Fingerprint MKBP events happen.
  void DoEnrollImageEvent(InternalRecord record, uint32_t event);
  void DoEnrollFingerUpEvent(InternalRecord record, uint32_t event);
  void DoMatchEvent(int attempt, uint32_t event);
  void DoMatchFingerUpEvent(uint32_t event);

  void KillMcuSession();

  void OnTaskComplete();

  bool LoadRecord(const std::string& user_id,
                  const std::string& label,
                  const std::string& record_id,
                  const base::Value& data);
  bool WriteRecord(const BiometricsManager::Record& record,
                   uint8_t* tmpl_data,
                   size_t tmpl_size);

  std::unique_ptr<CrosFpDevice> cros_dev_;

  SessionAction next_session_action_;

  // This list of records should be matching the templates loaded on the MCU.
  std::vector<InternalRecord> records_;

  BiometricsManager::EnrollScanDoneCallback on_enroll_scan_done_;
  BiometricsManager::AuthScanDoneCallback on_auth_scan_done_;
  BiometricsManager::SessionFailedCallback on_session_failed_;

  base::WeakPtrFactory<CrosFpBiometricsManager> session_weak_factory_;
  base::WeakPtrFactory<CrosFpBiometricsManager> weak_factory_;

  std::unique_ptr<BiodMetrics> biod_metrics_;
  std::unique_ptr<PowerButtonFilterInterface> power_button_filter_;
  BiodStorage biod_storage_;

  DISALLOW_COPY_AND_ASSIGN(CrosFpBiometricsManager);
};

}  // namespace biod

#endif  // BIOD_CROS_FP_BIOMETRICS_MANAGER_H_
