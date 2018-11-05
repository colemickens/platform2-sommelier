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

#include <sys/ioctl.h>

#include <base/files/file_util.h>
#include <base/values.h>

#include "biod/biod_storage.h"
extern "C" {
// Cros EC host commands definition as used by cros_fp.
#include "biod/ec/cros_ec_dev.h"
#include "biod/ec/ec_commands.h"
}

namespace biod {

class BiodMetrics;

class CrosFpBiometricsManager : public BiometricsManager {
 public:
  static std::unique_ptr<BiometricsManager> Create();

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

  bool ResetEntropy() override;

  // Kernel device exposing the MCU command interface.
  static constexpr char kCrosFpPath[] = "/dev/cros_fp";

 protected:
  void EndEnrollSession() override;
  void EndAuthSession() override;

 private:
  using SessionAction = base::Callback<void(const uint32_t event)>;

  class CrosFpDevice;

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

  CrosFpBiometricsManager();
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
  BiodStorage biod_storage_;

  DISALLOW_COPY_AND_ASSIGN(CrosFpBiometricsManager);
};

// Empty request or response for the EcCommand template below.
struct EmptyParam {};
// empty struct is one byte in C++, get the size we want instead.
template <typename T>
constexpr size_t realsizeof() {
  return std::is_empty<T>::value ? 0 : sizeof(T);
}

// Helper to build and send the command structures for cros_fp.
template <typename O, typename I>
class EcCommand {
 public:
  explicit EcCommand(uint32_t cmd, uint32_t ver = 0, const O& req = {})
      : data_({
            .cmd = {.version = ver,
                    .command = cmd,
                    .result = 0xff,
                    .outsize = realsizeof<O>(),
                    .insize = realsizeof<I>()},
            .req = req,
        }) {}

  void SetRespSize(uint32_t insize) { data_.cmd.insize = insize; }
  void SetReqSize(uint32_t outsize) { data_.cmd.outsize = outsize; }
  void SetReq(const O& req) { data_.req = req; }

  bool Run(int ec_fd) {
    data_.cmd.result = 0xff;
    int result = ioctl(ec_fd, CROS_EC_DEV_IOCXCMD_V2, &data_);
    if (result < 0) {
      PLOG(ERROR) << "FPMCU ioctl failed, command: " << data_.cmd.command;
      return false;
    }

    return (static_cast<uint32_t>(result) == data_.cmd.insize);
  }

  I* Resp() { return &data_.resp; }
  O* Req() { return &data_.req; }
  uint16_t Result() { return data_.cmd.result; }

 private:
  struct {
    struct cros_ec_command_v2 cmd;
    union {
      O req;
      I resp;
    };
  } data_;

  DISALLOW_COPY_AND_ASSIGN(EcCommand);
};

}  // namespace biod

#endif  // BIOD_CROS_FP_BIOMETRICS_MANAGER_H_
