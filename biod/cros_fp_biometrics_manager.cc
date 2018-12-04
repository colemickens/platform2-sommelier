// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/cros_fp_biometrics_manager.h"

#include <utility>

#include <errno.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/base64.h>
#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/string_piece.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <crypto/random.h>
#include <metrics/metrics_library.h>

#include "biod/biod_metrics.h"

namespace {

std::string MatchResultToString(int result) {
  switch (result) {
    case EC_MKBP_FP_ERR_MATCH_NO:
      return "No match";
    case EC_MKBP_FP_ERR_MATCH_NO_INTERNAL:
      return "Internal error";
    case EC_MKBP_FP_ERR_MATCH_NO_TEMPLATES:
      return "No templates";
    case EC_MKBP_FP_ERR_MATCH_NO_LOW_QUALITY:
      return "Low quality";
    case EC_MKBP_FP_ERR_MATCH_NO_LOW_COVERAGE:
      return "Low coverage";
    case EC_MKBP_FP_ERR_MATCH_YES:
      return "Finger matched";
    case EC_MKBP_FP_ERR_MATCH_YES_UPDATED:
      return "Finger matched, template updated";
    case EC_MKBP_FP_ERR_MATCH_YES_UPDATE_FAILED:
      return "Finger matched, template updated failed";
    default:
      return "Unknown matcher result";
  }
}

std::string EnrollResultToString(int result) {
  switch (result) {
    case EC_MKBP_FP_ERR_ENROLL_OK:
      return "Success";
    case EC_MKBP_FP_ERR_ENROLL_LOW_QUALITY:
      return "Low quality";
    case EC_MKBP_FP_ERR_ENROLL_IMMOBILE:
      return "Same area";
    case EC_MKBP_FP_ERR_ENROLL_LOW_COVERAGE:
      return "Low coverage";
    case EC_MKBP_FP_ERR_ENROLL_INTERNAL:
      return "Internal error";
    default:
      return "Unknown enrollment result";
  }
}

};  // namespace

namespace biod {

const std::string& CrosFpBiometricsManager::Record::GetId() const {
  CHECK(biometrics_manager_);
  CHECK(index_ < biometrics_manager_->records_.size());
  local_record_id_ = biometrics_manager_->records_[index_].record_id;
  return local_record_id_;
}

const std::string& CrosFpBiometricsManager::Record::GetUserId() const {
  CHECK(biometrics_manager_);
  CHECK(index_ <= biometrics_manager_->records_.size());
  local_user_id_ = biometrics_manager_->records_[index_].user_id;
  return local_user_id_;
}

const std::string& CrosFpBiometricsManager::Record::GetLabel() const {
  CHECK(biometrics_manager_);
  CHECK(index_ < biometrics_manager_->records_.size());
  local_label_ = biometrics_manager_->records_[index_].label;
  return local_label_;
}

bool CrosFpBiometricsManager::Record::SetLabel(std::string label) {
  CHECK(biometrics_manager_);
  CHECK(index_ < biometrics_manager_->records_.size());
  std::string old_label = biometrics_manager_->records_[index_].label;

  VendorTemplate tmpl;
  // TODO(vpalatin): would be faster to read it from disk
  if (!biometrics_manager_->cros_dev_->GetTemplate(index_, &tmpl))
    return false;
  biometrics_manager_->records_[index_].label = std::move(label);

  if (!biometrics_manager_->WriteRecord(*this, tmpl.data(), tmpl.size())) {
    biometrics_manager_->records_[index_].label = std::move(old_label);
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::Record::Remove() {
  if (!biometrics_manager_)
    return false;
  if (index_ >= biometrics_manager_->records_.size())
    return false;

  std::vector<InternalRecord>::iterator record =
      biometrics_manager_->records_.begin() + index_;
  std::string user_id = record->user_id;

  // TODO(mqg): only delete record if user_id is primary user.
  if (!biometrics_manager_->biod_storage_.DeleteRecord(user_id,
                                                       record->record_id))
    return false;

  // We cannot remove only one record if we want to stay in sync with the MCU,
  // Clear and reload everything.
  biometrics_manager_->records_.clear();
  biometrics_manager_->cros_dev_->SetContext(user_id);
  return biometrics_manager_->biod_storage_.ReadRecordsForSingleUser(user_id);
}

std::unique_ptr<BiometricsManager> CrosFpBiometricsManager::Create() {
  std::unique_ptr<CrosFpBiometricsManager> biometrics_manager(
      new CrosFpBiometricsManager);
  if (!biometrics_manager->Init())
    return nullptr;

  return biometrics_manager;
}

BiometricType CrosFpBiometricsManager::GetType() {
  return BIOMETRIC_TYPE_FINGERPRINT;
}

BiometricsManager::EnrollSession CrosFpBiometricsManager::StartEnrollSession(
    std::string user_id, std::string label) {
  LOG(INFO) << __func__;
  // Another session is on-going, fail early ...
  if (!next_session_action_.is_null())
    return BiometricsManager::EnrollSession();

  if (records_.size() >= cros_dev_->MaxTemplateCount()) {
    LOG(ERROR) << "No space for an additional template.";
    return BiometricsManager::EnrollSession();
  }

  if (!RequestEnrollImage(InternalRecord{biod_storage_.GenerateNewRecordId(),
                                         std::move(user_id), std::move(label)}))
    return BiometricsManager::EnrollSession();

  return BiometricsManager::EnrollSession(session_weak_factory_.GetWeakPtr());
}

BiometricsManager::AuthSession CrosFpBiometricsManager::StartAuthSession() {
  LOG(INFO) << __func__;
  // Another session is on-going, fail early ...
  if (!next_session_action_.is_null())
    return BiometricsManager::AuthSession();

  if (!RequestMatch())
    return BiometricsManager::AuthSession();

  return BiometricsManager::AuthSession(session_weak_factory_.GetWeakPtr());
}

std::vector<std::unique_ptr<BiometricsManager::Record>>
CrosFpBiometricsManager::GetRecords() {
  std::vector<std::unique_ptr<BiometricsManager::Record>> records;
  for (int i = 0; i < records_.size(); i++)
    records.emplace_back(std::unique_ptr<BiometricsManager::Record>(
        new Record(weak_factory_.GetWeakPtr(), i)));
  return records;
}

bool CrosFpBiometricsManager::DestroyAllRecords() {
  // Enumerate through records_ and delete each record.
  bool delete_all_records = true;
  for (auto& record : records_) {
    delete_all_records &=
        biod_storage_.DeleteRecord(record.user_id, record.record_id);
  }
  RemoveRecordsFromMemory();
  return delete_all_records;
}

void CrosFpBiometricsManager::RemoveRecordsFromMemory() {
  records_.clear();
  cros_dev_->ResetContext();
}

bool CrosFpBiometricsManager::ReadRecords(
    const std::unordered_set<std::string>& user_ids) {
  // TODO(mqg): delete this function and adjust parent class accordingly.
  LOG(WARNING) << __func__ << " should not be used.";
  return false;
}

bool CrosFpBiometricsManager::ReadRecordsForSingleUser(
    const std::string& user_id) {
  cros_dev_->SetContext(user_id);
  return biod_storage_.ReadRecordsForSingleUser(user_id);
}

void CrosFpBiometricsManager::SetEnrollScanDoneHandler(
    const BiometricsManager::EnrollScanDoneCallback& on_enroll_scan_done) {
  on_enroll_scan_done_ = on_enroll_scan_done;
}

void CrosFpBiometricsManager::SetAuthScanDoneHandler(
    const BiometricsManager::AuthScanDoneCallback& on_auth_scan_done) {
  on_auth_scan_done_ = on_auth_scan_done;
}

void CrosFpBiometricsManager::SetSessionFailedHandler(
    const BiometricsManager::SessionFailedCallback& on_session_failed) {
  on_session_failed_ = on_session_failed;
}

bool CrosFpBiometricsManager::SendStatsOnLogin() {
  bool rc = true;
  rc = biod_metrics_->SendEnrolledFingerCount(records_.size()) && rc;
  // Even though it looks a bit redundant with the finger count, it's easier to
  // discover and interpret.
  rc = biod_metrics_->SendFpUnlockEnabled(!records_.empty()) && rc;
  return rc;
}

void CrosFpBiometricsManager::SetDiskAccesses(bool allow) {
  biod_storage_.set_allow_access(allow);
}

bool CrosFpBiometricsManager::ResetSensor() {
  if (!cros_dev_->FpMode(FP_MODE_RESET_SENSOR)) {
    LOG(ERROR) << "Failed to send reset_sensor command to FPMCU.";
    return false;
  }

  int retries = 50;
  bool reset_complete = false;
  while (retries--) {
    uint32_t cur_mode;
    if (!cros_dev_->GetFpMode(&cur_mode)) {
      LOG(ERROR) << "Failed to query sensor state during reset.";
      return false;
    }

    if (!(cur_mode & FP_MODE_RESET_SENSOR)) {
      reset_complete = true;
      break;
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }

  if (!reset_complete) {
    LOG(ERROR) << "Reset on FPMCU failed to complete.";
    return false;
  }

  if (!Init()) {
    LOG(ERROR) << "Failed to reinitialize CrosFpBiometricsManager.";
    return false;
  }

  return true;
}

bool CrosFpBiometricsManager::ResetEntropy(bool factory_init) {
  bool success = cros_dev_->InitEntropy(!factory_init);
  if (!success) {
    LOG(INFO) << "Entropy source reset failed.";
    return false;
  }
  LOG(INFO) << "Entropy source has been successfully reset.";
  return true;
}

void CrosFpBiometricsManager::EndEnrollSession() {
  LOG(INFO) << __func__;
  KillMcuSession();
}

void CrosFpBiometricsManager::EndAuthSession() {
  LOG(INFO) << __func__;
  KillMcuSession();
}

void CrosFpBiometricsManager::KillMcuSession() {
  // TODO(vpalatin): test cros_dev_->FpMode(FP_MODE_DEEPSLEEP);
  cros_dev_->FpMode(0);
  session_weak_factory_.InvalidateWeakPtrs();
  OnTaskComplete();
}

CrosFpBiometricsManager::CrosFpBiometricsManager()
    : session_weak_factory_(this),
      weak_factory_(this),
      biod_metrics_(std::make_unique<BiodMetrics>()),
      biod_storage_(kCrosFpBiometricsManagerName,
                    base::Bind(&CrosFpBiometricsManager::LoadRecord,
                               base::Unretained(this))) {}

CrosFpBiometricsManager::~CrosFpBiometricsManager() {}

bool CrosFpBiometricsManager::Init() {
  cros_dev_ = CrosFpDevice::Open(base::Bind(
      &CrosFpBiometricsManager::OnMkbpEvent, base::Unretained(this)));
  return !!cros_dev_;
}

void CrosFpBiometricsManager::OnEnrollScanDone(
    ScanResult result, const BiometricsManager::EnrollStatus& enroll_status) {
  if (!on_enroll_scan_done_.is_null())
    on_enroll_scan_done_.Run(result, enroll_status);
}

void CrosFpBiometricsManager::OnAuthScanDone(
    ScanResult result, const BiometricsManager::AttemptMatches& matches) {
  if (!on_auth_scan_done_.is_null())
    on_auth_scan_done_.Run(result, matches);
}

void CrosFpBiometricsManager::OnSessionFailed() {
  if (!on_session_failed_.is_null())
    on_session_failed_.Run();
}

void CrosFpBiometricsManager::OnMkbpEvent(uint32_t event) {
  if (!next_session_action_.is_null())
    next_session_action_.Run(event);
}

bool CrosFpBiometricsManager::RequestEnrollImage(InternalRecord record) {
  next_session_action_ =
      base::Bind(&CrosFpBiometricsManager::DoEnrollImageEvent,
                 base::Unretained(this), std::move(record));
  if (!cros_dev_->FpMode(FP_MODE_ENROLL_SESSION | FP_MODE_ENROLL_IMAGE)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to start enrolling mode";
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::RequestEnrollFingerUp(InternalRecord record) {
  next_session_action_ =
      base::Bind(&CrosFpBiometricsManager::DoEnrollFingerUpEvent,
                 base::Unretained(this), std::move(record));
  if (!cros_dev_->FpMode(FP_MODE_ENROLL_SESSION | FP_MODE_FINGER_UP)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to wait for finger up";
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::RequestMatch(int attempt) {
  next_session_action_ = base::Bind(&CrosFpBiometricsManager::DoMatchEvent,
                                    base::Unretained(this), attempt);
  if (!cros_dev_->FpMode(FP_MODE_MATCH)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to start matching mode";
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::RequestMatchFingerUp() {
  next_session_action_ = base::Bind(
      &CrosFpBiometricsManager::DoMatchFingerUpEvent, base::Unretained(this));
  if (!cros_dev_->FpMode(FP_MODE_FINGER_UP)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to request finger up event";
    return false;
  }
  return true;
}

void CrosFpBiometricsManager::DoEnrollImageEvent(InternalRecord record,
                                                 uint32_t event) {
  if (!(event & EC_MKBP_FP_ENROLL)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }

  int image_result = EC_MKBP_FP_ERRCODE(event);
  LOG(INFO) << __func__ << " result: '" << EnrollResultToString(image_result)
            << "'";
  ScanResult scan_result;
  switch (image_result) {
    case EC_MKBP_FP_ERR_ENROLL_OK:
      scan_result = ScanResult::SCAN_RESULT_SUCCESS;
      break;
    case EC_MKBP_FP_ERR_ENROLL_IMMOBILE:
      scan_result = ScanResult::SCAN_RESULT_IMMOBILE;
      break;
    case EC_MKBP_FP_ERR_ENROLL_LOW_COVERAGE:
      scan_result = ScanResult::SCAN_RESULT_PARTIAL;
      break;
    case EC_MKBP_FP_ERR_ENROLL_LOW_QUALITY:
      scan_result = ScanResult::SCAN_RESULT_INSUFFICIENT;
      break;
    case EC_MKBP_FP_ERR_ENROLL_INTERNAL:
    default:
      LOG(ERROR) << "Unexpected result from capture: " << std::hex << event;
      OnSessionFailed();
      return;
  }

  int percent = EC_MKBP_FP_ENROLL_PROGRESS(event);

  if (percent < 100) {
    BiometricsManager::EnrollStatus enroll_status = {false, percent};

    OnEnrollScanDone(scan_result, enroll_status);

    // The user needs to remove the finger before the next enrollment image.
    if (!RequestEnrollFingerUp(std::move(record)))
      OnSessionFailed();

    return;
  }

  // we are done with captures, save the template.
  OnTaskComplete();

  VendorTemplate tmpl;
  if (!cros_dev_->GetTemplate(CrosFpDevice::kLastTemplate, &tmpl)) {
    LOG(ERROR) << "Failed to retrieve enrolled finger";
    OnSessionFailed();
    return;
  }

  records_.emplace_back(record);
  Record current_record(weak_factory_.GetWeakPtr(), records_.size() - 1);
  if (!WriteRecord(current_record, tmpl.data(), tmpl.size())) {
    records_.pop_back();
    OnSessionFailed();
    return;
  }

  BiometricsManager::EnrollStatus enroll_status = {true, 100};
  OnEnrollScanDone(ScanResult::SCAN_RESULT_SUCCESS, enroll_status);
}

void CrosFpBiometricsManager::DoEnrollFingerUpEvent(InternalRecord record,
                                                    uint32_t event) {
  if (!(event & EC_MKBP_FP_FINGER_UP)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }

  if (!RequestEnrollImage(std::move(record)))
    OnSessionFailed();
}

void CrosFpBiometricsManager::DoMatchFingerUpEvent(uint32_t event) {
  if (!(event & EC_MKBP_FP_FINGER_UP)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }
  // The user has lifted their finger, try to match the next touch.
  if (!RequestMatch())
    OnSessionFailed();
}

void CrosFpBiometricsManager::DoMatchEvent(int attempt, uint32_t event) {
  if (!(event & EC_MKBP_FP_MATCH)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }

  ScanResult result;
  int match_result = EC_MKBP_FP_ERRCODE(event);

  // If the finger is positioned slightly off the sensor, retry a few times
  // before failing. Typically the user has put their finger down and is now
  // moving their finger to the correct position on the sensor. Instead of
  // immediately failing, retry until we get a good image.
  // Retry 20 times, which takes about 5 to 15s, before giving up and sending
  // an error back to the user. Assume ~1s for user noticing that no matching
  // happened, some time to move the finger on the sensor to allow a full
  // capture and another 1s for the second matching attempt. 5s gives a bit of
  // margin to avoid interrupting the user while they're moving the finger on
  // the sensor.
  const int kMaxPartialAttempts = 20;

  if (match_result == EC_MKBP_FP_ERR_MATCH_NO_LOW_COVERAGE &&
      attempt < kMaxPartialAttempts) {
    /* retry a match */
    if (!RequestMatch(attempt + 1))
      OnSessionFailed();
    return;
  }

  // Don't try to match again until the user has lifted their finger from the
  // sensor. Request the FingerUp event as soon as the HW signaled a match so it
  // doesn't attempt a new match while the host is processing the first
  // match event.
  if (!RequestMatchFingerUp()) {
    OnSessionFailed();
    return;
  }

  std::vector<int> dirty_list;
  if (match_result == EC_MKBP_FP_ERR_MATCH_YES_UPDATED) {
    std::bitset<32> dirty_bitmap(0);
    // Retrieve which templates have been updated.
    if (!cros_dev_->GetDirtyMap(&dirty_bitmap))
      LOG(ERROR) << "Failed to get updated templates map";
    // Create a list of modified template indexes from the bitmap.
    dirty_list.reserve(dirty_bitmap.count());
    for (int i = 0; dirty_bitmap.any() && i < dirty_bitmap.size(); i++)
      if (dirty_bitmap[i]) {
        dirty_list.emplace_back(i);
        dirty_bitmap.reset(i);
      }
  }

  BiometricsManager::AttemptMatches matches;
  std::vector<std::string> records;

  uint32_t match_idx = EC_MKBP_FP_MATCH_IDX(event);
  LOG(INFO) << __func__ << " result: '" << MatchResultToString(match_result)
            << "' (finger: " << match_idx << ")";
  switch (match_result) {
    case EC_MKBP_FP_ERR_MATCH_NO_TEMPLATES:
      LOG(ERROR) << "No templates to match: " << std::hex << event;
      result = ScanResult::SCAN_RESULT_SUCCESS;
      break;
    case EC_MKBP_FP_ERR_MATCH_NO_INTERNAL:
      LOG(ERROR) << "Internal error when matching templates: " << std::hex
                 << event;
      result = ScanResult::SCAN_RESULT_SUCCESS;
      break;
    case EC_MKBP_FP_ERR_MATCH_NO:
      // This is the API: empty matches but still SCAN_RESULT_SUCCESS.
      result = ScanResult::SCAN_RESULT_SUCCESS;
      break;
    case EC_MKBP_FP_ERR_MATCH_YES:
    case EC_MKBP_FP_ERR_MATCH_YES_UPDATED:
    case EC_MKBP_FP_ERR_MATCH_YES_UPDATE_FAILED:
      result = ScanResult::SCAN_RESULT_SUCCESS;

      if (match_idx < records_.size()) {
        records.push_back(records_[match_idx].record_id);
        matches.emplace(records_[match_idx].user_id, std::move(records));
      } else {
        LOG(ERROR) << "Invalid finger index " << match_idx;
      }
      break;
    case EC_MKBP_FP_ERR_MATCH_NO_LOW_QUALITY:
      result = ScanResult::SCAN_RESULT_INSUFFICIENT;
      break;
    case EC_MKBP_FP_ERR_MATCH_NO_LOW_COVERAGE:
      result = ScanResult::SCAN_RESULT_PARTIAL;
      break;
    default:
      LOG(ERROR) << "Unexpected result from matching templates: " << std::hex
                 << event;
      OnSessionFailed();
      return;
  }

  // Send back the result directly (as we are running on the main thread).
  OnAuthScanDone(result, std::move(matches));

  int capture_ms, matcher_ms, overall_ms;
  if (cros_dev_->GetFpStats(&capture_ms, &matcher_ms, &overall_ms)) {
    // SCAN_RESULT_SUCCESS and EC_MKBP_FP_ERR_MATCH_NO means "no match".
    bool matched = (result == ScanResult::SCAN_RESULT_SUCCESS &&
                    match_result != EC_MKBP_FP_ERR_MATCH_NO);
    biod_metrics_->SendFpLatencyStats(matched, capture_ms, matcher_ms,
                                      overall_ms);
  }

  // Record updated templates
  // TODO(vpalatin): this is slow, move to end of session ?
  for (int i : dirty_list) {
    VendorTemplate templ;
    bool rc = cros_dev_->GetTemplate(i, &templ);
    LOG(INFO) << "Retrieve updated template " << i << " -> " << rc;
    if (!rc)
      continue;

    Record current_record(weak_factory_.GetWeakPtr(), i);
    if (!WriteRecord(current_record, templ.data(), templ.size())) {
      LOG(ERROR) << "Cannot update record " << records_[i].record_id
                 << " in storage during AuthSession because writing failed.";
    }
  }
}

void CrosFpBiometricsManager::OnTaskComplete() {
  next_session_action_ = SessionAction();
}

bool CrosFpBiometricsManager::LoadRecord(const std::string& user_id,
                                         const std::string& label,
                                         const std::string& record_id,
                                         const base::Value& data) {
  std::string tmpl_data_base64;
  if (!data.GetAsString(&tmpl_data_base64)) {
    LOG(ERROR) << "Cannot load data string from record " << record_id << ".";
    return false;
  }

  base::StringPiece tmpl_data_base64_sp(tmpl_data_base64);
  std::string tmpl_data_str;
  base::Base64Decode(tmpl_data_base64_sp, &tmpl_data_str);

  if (records_.size() >= cros_dev_->MaxTemplateCount()) {
    LOG(ERROR) << "No space to upload template from " << record_id << ".";
    return false;
  }

  LOG(INFO) << "Upload record " << record_id;
  VendorTemplate tmpl(tmpl_data_str.begin(), tmpl_data_str.end());
  auto* metadata =
      reinterpret_cast<const ec_fp_template_encryption_metadata*>(tmpl.data());
  if (metadata->struct_version != cros_dev_->TemplateVersion()) {
    LOG(ERROR) << "Version mismatch between template ("
               << metadata->struct_version << ") and hardware ("
               << cros_dev_->TemplateVersion() << ")";
    biod_storage_.DeleteRecord(user_id, record_id);
    return false;
  }
  if (!cros_dev_->UploadTemplate(tmpl)) {
    LOG(ERROR) << "Cannot send template to the MCU from " << record_id << ".";
    return false;
  }

  InternalRecord internal_record = {record_id, user_id, label};
  records_.emplace_back(std::move(internal_record));
  return true;
}

bool CrosFpBiometricsManager::WriteRecord(
    const BiometricsManager::Record& record,
    uint8_t* tmpl_data,
    size_t tmpl_size) {
  base::StringPiece tmpl_sp(reinterpret_cast<char*>(tmpl_data), tmpl_size);
  std::string tmpl_base64;
  base::Base64Encode(tmpl_sp, &tmpl_base64);

  return biod_storage_.WriteRecord(record,
                                   std::make_unique<base::Value>(tmpl_base64));
}
}  // namespace biod
