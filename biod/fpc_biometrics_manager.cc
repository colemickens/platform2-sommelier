// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/fpc_biometrics_manager.h"

#include <algorithm>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/base64.h>
#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/string_piece.h>
#include <base/threading/thread_task_runner_handle.h>

#include "biod/fpc/fp_sensor.h"

namespace biod {

class FpcBiometricsManager::SensorLibrary {
 public:
  ~SensorLibrary();

  static std::unique_ptr<SensorLibrary> Open(
      const std::shared_ptr<BioLibrary>& bio_lib, int fd);

  BioEnrollment BeginEnrollment();
  std::tuple<int, BioImage> AcquireImage();
  bool WaitFingerUp();
  bool Cancel();

 private:
  using fp_sensor_open_fp = int (*)(int fd);
  using fp_sensor_close_fp = int (*)(void);
  using fp_sensor_get_model_fp = int (*)(uint32_t* vendor_id,
                                         uint32_t* product_id,
                                         uint32_t* model_id,
                                         uint32_t* version);
  using fp_sensor_get_pixel_format_fp = int (*)(uint32_t* pixel_format);
  using fp_sensor_get_image_data_size_fp = ssize_t (*)(void);
  using fp_sensor_get_image_dimensions_fp = int (*)(uint32_t* width,
                                                    uint32_t* height);
  using fp_sensor_acquire_image_fp = int (*)(uint8_t* image_data, size_t size);
  using fp_sensor_wait_finger_up_fp = int (*)(void);
  using fp_sensor_cancel_fp = int (*)(void);

  explicit SensorLibrary(const std::shared_ptr<BioLibrary>& bio_lib)
      : bio_lib_(bio_lib) {}
  bool Init(int fd);

  fp_sensor_open_fp open_;
  fp_sensor_close_fp close_;
  fp_sensor_get_model_fp get_model_;
  fp_sensor_get_pixel_format_fp get_pixel_format_;
  fp_sensor_get_image_data_size_fp get_image_data_size_;
  fp_sensor_get_image_dimensions_fp get_image_dimensions_;
  fp_sensor_acquire_image_fp acquire_image_;
  fp_sensor_wait_finger_up_fp wait_finger_up_;
  fp_sensor_cancel_fp cancel_;

  std::shared_ptr<BioLibrary> bio_lib_;
  bool needs_close_ = false;
  size_t image_data_size_ = 0;
  BioSensor bio_sensor_;

  DISALLOW_COPY_AND_ASSIGN(SensorLibrary);
};

FpcBiometricsManager::SensorLibrary::~SensorLibrary() {
  if (needs_close_)
    close_();
}

std::unique_ptr<FpcBiometricsManager::SensorLibrary>
FpcBiometricsManager::SensorLibrary::Open(
    const std::shared_ptr<BioLibrary>& bio_lib, int fd) {
  std::unique_ptr<SensorLibrary> lib(new SensorLibrary(bio_lib));
  if (!lib->Init(fd))
    return nullptr;
  return lib;
}

BioEnrollment FpcBiometricsManager::SensorLibrary::BeginEnrollment() {
  return bio_sensor_.BeginEnrollment();
}

std::tuple<int, BioImage> FpcBiometricsManager::SensorLibrary::AcquireImage() {
  std::vector<uint8_t> image_data(image_data_size_);
  int acquire_result = acquire_image_(image_data.data(), image_data.size());
  if (acquire_result)
    return std::tuple<int, BioImage>(acquire_result, BioImage());

  BioImage image = bio_sensor_.CreateImage();
  if (!image || !image.SetData(&image_data)) {
    LOG(ERROR) << "Failed to construct BioImage for image acquired.";
    return std::tuple<int, BioImage>(-ENOMEM, BioImage());
  }

  return std::tuple<int, BioImage>(0 /* success */, std::move(image));
}

bool FpcBiometricsManager::SensorLibrary::WaitFingerUp() {
  int ret = wait_finger_up_();
  if (ret)
    LOG(ERROR) << "Failed to wait for finger up: " << ret;
  return ret == 0;
}

bool FpcBiometricsManager::SensorLibrary::Cancel() {
  int ret = cancel_();
  if (ret)
    LOG(ERROR) << "Failed to cancel FPC sensor operation: " << ret;
  return ret == 0;
}

bool FpcBiometricsManager::SensorLibrary::Init(int fd) {
#define SENSOR_SYM(x)                                                  \
  do {                                                                 \
    x##_ = bio_lib_->GetFunction<fp_sensor_##x##_fp>("fp_sensor_" #x); \
    if (!x##_) {                                                       \
      LOG(ERROR) << #x " is missing from library";                     \
      return false;                                                    \
    }                                                                  \
  } while (0)

  // Here we use the very same shared object loaded by BioLibrary to pull out
  // some private functions that interface with the FPC sensor.
  SENSOR_SYM(open);
  SENSOR_SYM(close);
  SENSOR_SYM(get_model);
  SENSOR_SYM(get_pixel_format);
  SENSOR_SYM(get_image_data_size);
  SENSOR_SYM(get_image_dimensions);
  SENSOR_SYM(acquire_image);
  SENSOR_SYM(wait_finger_up);
  SENSOR_SYM(cancel);

#undef SENSOR_SYM

  int ret = open_(fd);
  if (ret) {
    LOG(ERROR) << "Failed to open sensor library: " << ret;
    return false;
  }
  needs_close_ = true;

  BioSensor::Model model;
  ret = get_model_(
      &model.vendor_id, &model.product_id, &model.model_id, &model.version);
  if (ret) {
    LOG(ERROR) << "Failed to get sensor model: " << ret;
    return false;
  }

  uint32_t pixel_format;
  ret = get_pixel_format_(&pixel_format);
  if (ret) {
    LOG(ERROR) << "Failed to get sensor pixel format: " << ret;
    return false;
  }

  ssize_t image_data_size = get_image_data_size_();
  if (image_data_size <= 0) {
    LOG(ERROR) << "Failed to get sensor image data size: " << image_data_size;
    return false;
  }
  image_data_size_ = image_data_size;

  uint32_t width, height;
  ret = get_image_dimensions_(&width, &height);
  if (ret) {
    LOG(ERROR) << "Failed to get sensor image dimensions: " << ret;
    return false;
  }

  LOG(INFO) << "FPC Sensor Info ";
  LOG(INFO) << "  Vendor ID  : " << model.vendor_id;
  LOG(INFO) << "  Product ID : " << model.product_id;
  LOG(INFO) << "  Model ID   : " << model.model_id;
  LOG(INFO) << "  Version    : " << model.version;
  LOG(INFO) << "FPC Image Info ";
  // Prints the pixel format in FOURCC format.
  const uint32_t a = pixel_format;
  LOG(INFO) << "  Pixel Format     : " << static_cast<char>(a)
            << static_cast<char>(a >> 8) << static_cast<char>(a >> 16)
            << static_cast<char>(a >> 24);
  LOG(INFO) << "  Image Data Size  : " << image_data_size;
  LOG(INFO) << "  Image Dimensions : " << width << "x" << height;

  bio_sensor_ = bio_lib_->CreateSensor();
  if (!bio_sensor_)
    return false;

  if (!bio_sensor_.SetModel(model))
    return false;

  if (!bio_sensor_.SetFormat(pixel_format))
    return false;

  if (!bio_sensor_.SetSize(width, height))
    return false;

  return true;
}

const std::string& FpcBiometricsManager::Record::GetId() const {
  return id_;
}

const std::string& FpcBiometricsManager::Record::GetUserId() const {
  CHECK(WithInternal([this](RecordIterator i) {
    this->local_user_id_ = i->second.user_id;
  })) << ": Attempted to get user ID for invalid BiometricsManager Record";
  return local_user_id_;
}

const std::string& FpcBiometricsManager::Record::GetLabel() const {
  CHECK(WithInternal([this](RecordIterator i) {
    this->local_label_ = i->second.label;
  })) << ": Attempted to get label for invalid BiometricsManager Record";
  return local_label_;
}

bool FpcBiometricsManager::Record::SetLabel(std::string label) {
  std::string old_label;
  std::vector<uint8_t> serialized_tmpl;
  CHECK(WithInternal([&](RecordIterator i) {
    old_label = i->second.label;
    i->second.label = std::move(label);
    (i->second.tmpl).Serialize(&serialized_tmpl);
  })) << ": Attempted to reset label for invalid BiometricsManager Record";

  if (!biometrics_manager_->WriteRecord(
          *this, serialized_tmpl.data(), serialized_tmpl.size())) {
    CHECK(WithInternal(
        [&](RecordIterator i) { i->second.label = std::move(old_label); }));
    return false;
  }
  return true;
}

bool FpcBiometricsManager::Record::Remove() {
  if (!biometrics_manager_)
    return false;
  if (!biometrics_manager_->biod_storage_.DeleteRecord(GetUserId(), GetId())) {
    return false;
  }
  return WithInternal(
      [this](RecordIterator i) { biometrics_manager_->records_.erase(i); });
}

int FpcBiometricsManager::g_sensor_fd = -1;

std::unique_ptr<BiometricsManager> FpcBiometricsManager::Create() {
  std::unique_ptr<FpcBiometricsManager> biometrics_manager(
      new FpcBiometricsManager);
  if (!biometrics_manager->Init())
    return nullptr;

  return std::unique_ptr<BiometricsManager>(std::move(biometrics_manager));
}

BiometricType FpcBiometricsManager::GetType() {
  return BIOMETRIC_TYPE_FINGERPRINT;
}

BiometricsManager::EnrollSession FpcBiometricsManager::StartEnrollSession(
    std::string user_id, std::string label) {
  if (running_task_)
    return BiometricsManager::EnrollSession();

  std::shared_ptr<BioTemplate> tmpl = std::make_shared<BioTemplate>();

  kill_task_ = false;
  bool task_will_run = sensor_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FpcBiometricsManager::DoEnrollSessionTask,
                 base::Unretained(this),
                 base::ThreadTaskRunnerHandle::Get(),
                 tmpl),
      base::Bind(&FpcBiometricsManager::OnEnrollSessionComplete,
                 weak_factory_.GetWeakPtr(),
                 std::move(user_id),
                 std::move(label),
                 tmpl));

  if (!task_will_run) {
    LOG(ERROR) << "Failed to schedule EnrollSession task";
    return BiometricsManager::EnrollSession();
  }

  // Note that the On*Complete function sets running_task_ to false on this
  // thread's message loop, so setting it here does not result in a race
  // condition.
  running_task_ = true;

  return BiometricsManager::EnrollSession(session_weak_factory_.GetWeakPtr());
}

BiometricsManager::AuthSession FpcBiometricsManager::StartAuthSession() {
  if (running_task_)
    return BiometricsManager::AuthSession();

  std::shared_ptr<std::unordered_set<std::string>> updated_record_ids =
      std::make_shared<std::unordered_set<std::string>>();

  kill_task_ = false;
  bool task_will_run = sensor_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FpcBiometricsManager::DoAuthSessionTask,
                 base::Unretained(this),
                 base::ThreadTaskRunnerHandle::Get(),
                 updated_record_ids),
      base::Bind(&FpcBiometricsManager::OnAuthSessionComplete,
                 weak_factory_.GetWeakPtr(),
                 updated_record_ids));

  if (!task_will_run) {
    LOG(ERROR) << "Failed to schedule AuthSession task";
    return BiometricsManager::AuthSession();
  }

  // Note that the On*Complete function sets running_task_ to false on this
  // thread's message loop, so setting it here does not result in a race
  // condition.
  running_task_ = true;

  return BiometricsManager::AuthSession(session_weak_factory_.GetWeakPtr());
}

std::vector<std::unique_ptr<BiometricsManager::Record>>
FpcBiometricsManager::GetRecords() {
  base::AutoLock guard(records_lock_);
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

bool FpcBiometricsManager::DestroyAllRecords() {
  base::AutoLock guard(records_lock_);

  // Enumerate through records_ and delete each record.
  bool delete_all_records = true;
  for (auto& record_pair : records_) {
    std::string record_id(record_pair.first);
    std::string user_id(record_pair.second.user_id);
    delete_all_records &= biod_storage_.DeleteRecord(user_id, record_id);
  }
  records_.clear();
  return delete_all_records;
}

void FpcBiometricsManager::RemoveRecordsFromMemory() {
  base::AutoLock guard(records_lock_);
  records_.clear();
}

bool FpcBiometricsManager::ReadRecords(
    const std::unordered_set<std::string>& user_ids) {
  return biod_storage_.ReadRecords(user_ids);
}

void FpcBiometricsManager::SetEnrollScanDoneHandler(
    const BiometricsManager::EnrollScanDoneCallback& on_enroll_scan_done) {
  on_enroll_scan_done_ = on_enroll_scan_done;
}

void FpcBiometricsManager::SetAuthScanDoneHandler(
    const BiometricsManager::AuthScanDoneCallback& on_auth_scan_done) {
  on_auth_scan_done_ = on_auth_scan_done;
}

void FpcBiometricsManager::SetSessionFailedHandler(
    const BiometricsManager::SessionFailedCallback& on_session_failed) {
  on_session_failed_ = on_session_failed;
}

void FpcBiometricsManager::EndEnrollSession() {
  KillSensorTask();
}

void FpcBiometricsManager::EndAuthSession() {
  KillSensorTask();
}

void FpcBiometricsManager::KillSensorTask() {
  {
    base::AutoLock guard(kill_task_lock_);
    kill_task_ = true;
  }
  sensor_lib_->Cancel();
}

FpcBiometricsManager::FpcBiometricsManager()
    : sensor_thread_("fpc_sensor"),
      session_weak_factory_(this),
      weak_factory_(this),
      biod_storage_(kFpcBiometricsManagerName,
                    base::Bind(&FpcBiometricsManager::LoadRecord,
                               base::Unretained(this))) {}

FpcBiometricsManager::~FpcBiometricsManager() {}

bool FpcBiometricsManager::Init() {
  const char kFpcSensorPath[] = "/dev/fpc_sensor0";
  sensor_fd_ = base::ScopedFD(open(kFpcSensorPath, O_RDWR));
  g_sensor_fd = sensor_fd_.get();
  if (sensor_fd_.get() < 0) {
    LOG(ERROR) << "Failed to open " << kFpcSensorPath;
    return false;
  }

  const char kFpcLibName[] = "/opt/fpc/lib/libfpsensor.so";
  bio_lib_ = BioLibrary::Load(base::FilePath(kFpcLibName));
  if (!bio_lib_)
    return false;

  sensor_lib_ = SensorLibrary::Open(bio_lib_, sensor_fd_.get());
  if (!sensor_lib_)
    return false;

  if (!sensor_thread_.Start()) {
    LOG(ERROR) << "Failed to start sensor thread";
    return false;
  }

  return true;
}

void FpcBiometricsManager::OnEnrollScanDone(
    ScanResult result, const BiometricsManager::EnrollStatus& enroll_status) {
  if (!on_enroll_scan_done_.is_null())
    on_enroll_scan_done_.Run(result, enroll_status);
}

void FpcBiometricsManager::OnAuthScanDone(
    ScanResult result, const BiometricsManager::AttemptMatches& matches) {
  if (!on_auth_scan_done_.is_null())
    on_auth_scan_done_.Run(result, matches);
}

void FpcBiometricsManager::OnSessionFailed() {
  if (!on_session_failed_.is_null())
    on_session_failed_.Run();
}

FpcBiometricsManager::ScanData FpcBiometricsManager::ScanImage() {
  DCHECK(sensor_thread_.task_runner()->RunsTasksOnCurrentThread());
  bool success = sensor_lib_->WaitFingerUp();
  {
    base::AutoLock guard(kill_task_lock_);
    if (kill_task_)
      return ScanData(ScanData::Killed());
  }

  if (!success)
    return ScanData();

  int acquire_result, attempts = 0;
  BioImage image;
  // If the finger is positioned slightly off the sensor, retry a few times
  // before failing. Typically the user has put their finger down and is now
  // moving their finger to the correct position on the sensor. Instead of
  // immediately failing, retry until we get a good image.
  // Retry 20 times, which takes ~5s on Eve, before giving up and sending an
  // error back to the user. Assume ~1s for user noticing that no matching
  // happened, some time to move the finger on the sensor to allow a full
  // capture and another 1s for the second matching attempt. 5s gives a bit of
  // margin to avoid interrupting the user while they're moving the finger on
  // the sensor.
  const int kMaxPartialAttempts = 20;
  do {
    std::tie(acquire_result, image) = sensor_lib_->AcquireImage();
    {
      base::AutoLock guard(kill_task_lock_);
      if (kill_task_)
        return ScanData(ScanData::Killed());
    }
    ++attempts;
  } while (acquire_result == FP_SENSOR_LOW_SENSOR_COVERAGE &&
           attempts < kMaxPartialAttempts);

  switch (acquire_result) {
    case 0:
      break;
    case FP_SENSOR_TOO_FAST:
      return ScanData(ScanResult::SCAN_RESULT_TOO_FAST);
    case FP_SENSOR_LOW_IMAGE_QUALITY:
      return ScanData(ScanResult::SCAN_RESULT_INSUFFICIENT);
    case FP_SENSOR_LOW_SENSOR_COVERAGE:
      return ScanData(ScanResult::SCAN_RESULT_PARTIAL);
    default:
      LOG(ERROR) << "Unexpected result from acquiring image: "
                 << acquire_result;
      return ScanData();
  }

  return ScanData(std::move(image));
}

void FpcBiometricsManager::DoEnrollSessionTask(
    const TaskRunnerRef& task_runner,
    const std::shared_ptr<BioTemplate>& tmpl) {
  DCHECK(sensor_thread_.task_runner()->RunsTasksOnCurrentThread());

  if (kill_task_)
    return;

  BioEnrollment enrollment = sensor_lib_->BeginEnrollment();
  if (!enrollment)
    return;

  for (;;) {
    ScanData scan = ScanImage();

    // The previous function can return early if this task was killed, or there
    // was an unrecoverable hardware failure.
    if (scan.killed || !scan.success)
      return;

    ScanResult scan_result = scan.result;
    if (scan) {
      int add_result = enrollment.AddImage(scan.image);
      switch (add_result) {
        case BIO_ENROLLMENT_OK:
          break;
        case BIO_ENROLLMENT_IMMOBILE:
          scan_result = ScanResult::SCAN_RESULT_IMMOBILE;
          break;
        case BIO_ENROLLMENT_LOW_COVERAGE:
          scan_result = ScanResult::SCAN_RESULT_PARTIAL;
          break;
        case BIO_ENROLLMENT_LOW_QUALITY:
          scan_result = ScanResult::SCAN_RESULT_INSUFFICIENT;
          break;
        default:
          LOG(ERROR) << "Unexpected result from add image: " << add_result;
          return;
      }
    }

    int complete_result = enrollment.IsComplete();
    if (complete_result < 0) {
      LOG(ERROR) << "Failed to get enrollment completion: " << complete_result;
      return;
    } else if (complete_result == 1) {
      *tmpl = enrollment.Finish();
      return;
    } else {
      BiometricsManager::EnrollStatus enroll_status = {
          false, enrollment.GetPercentComplete()};

      // Notice we will only ever post the on_enroll_scan_done task on an
      // incomplete enrollment. The complete on_enroll_scan_done task is only
      // posted after the enrollment is added to the enrollments(records) map,
      // which is done only on the main thread's message loop.
      bool task_will_run = task_runner->PostTask(
          FROM_HERE,
          base::Bind(&FpcBiometricsManager::OnEnrollScanDone,
                     base::Unretained(this),
                     scan_result,
                     enroll_status));
      if (!task_will_run) {
        LOG(ERROR) << "Failed to schedule EnrollScanDone callback";
        return;
      }
    }
  }
}

void FpcBiometricsManager::OnEnrollSessionComplete(
    std::string user_id,
    std::string label,
    const std::shared_ptr<BioTemplate>& tmpl) {
  OnTaskComplete();

  if (kill_task_)
    return;

  // Remember tmpl stores a shared pointer to a /pointer/ which contains the
  // actual result.
  if (!*tmpl.get()) {
    OnSessionFailed();
    return;
  }

  std::vector<uint8_t> serialized_tmpl;
  if (!tmpl->Serialize(&serialized_tmpl)) {
    OnSessionFailed();
    return;
  }

  std::string record_id(biod_storage_.GenerateNewRecordId());
  {
    base::AutoLock guard(records_lock_);
    records_.emplace(
        record_id,
        InternalRecord{
            std::move(user_id), std::move(label), std::move(*tmpl.get())});
  }
  Record current_record(weak_factory_.GetWeakPtr(), record_id);
  if (!WriteRecord(
          current_record, serialized_tmpl.data(), serialized_tmpl.size())) {
    {
      base::AutoLock guard(records_lock_);
      records_.erase(record_id);
    }
    OnSessionFailed();
    return;
  }

  BiometricsManager::EnrollStatus enroll_status = {true, 100};
  OnEnrollScanDone(ScanResult::SCAN_RESULT_SUCCESS, enroll_status);
}

void FpcBiometricsManager::DoAuthSessionTask(
    const TaskRunnerRef& task_runner,
    std::shared_ptr<std::unordered_set<std::string>> updated_record_ids) {
  DCHECK(sensor_thread_.task_runner()->RunsTasksOnCurrentThread());

  if (kill_task_)
    return;

  BiometricsManager::AttemptMatches matches;

  for (;;) {
    ScanData scan = ScanImage();

    // The previous function can return early if this task was killed, or there
    // was an unrecoverable hardware failure.
    if (scan.killed || !scan.success)
      break;

    ScanResult result = scan.result;
    if (result == ScanResult::SCAN_RESULT_SUCCESS) {
      matches.clear();

      base::AutoLock guard(records_lock_);
      for (auto& kv : records_) {
        InternalRecord& record = kv.second;

        int match_result = record.tmpl.MatchImage(scan.image);
        switch (match_result) {
          case BIO_TEMPLATE_NO_MATCH:
            break;
          case BIO_TEMPLATE_MATCH_UPDATED:  // record.tmpl got updated
            updated_record_ids->insert(kv.first);
          case BIO_TEMPLATE_MATCH: {
            auto emplace_result =
                matches.emplace(record.user_id, std::vector<std::string>());
            emplace_result.first->second.emplace_back(kv.first);
            break;
          }
          case BIO_TEMPLATE_LOW_QUALITY:
            result = ScanResult::SCAN_RESULT_INSUFFICIENT;
            break;
          case BIO_TEMPLATE_LOW_COVERAGE:
            result = ScanResult::SCAN_RESULT_PARTIAL;
            break;
          default:
            LOG(ERROR) << "Unexpected result from matching templates: "
                       << match_result;
            return;
        }
      }
    }

    // Assuming there was at least one match, we don't want to bother the user
    // with error messages.
    if (!matches.empty())
      result = ScanResult::SCAN_RESULT_SUCCESS;

    bool task_will_run =
        task_runner->PostTask(FROM_HERE,
                              base::Bind(&FpcBiometricsManager::OnAuthScanDone,
                                         base::Unretained(this),
                                         result,
                                         std::move(matches)));
    if (!task_will_run) {
      LOG(ERROR) << "Failed to schedule AuthScanDone callback";
      return;
    }
  }
}

void FpcBiometricsManager::OnAuthSessionComplete(
    std::shared_ptr<std::unordered_set<std::string>> updated_record_ids) {
  OnTaskComplete();

  // AuthSession never ends except on error or being killed. If no kill
  // signal was given, we can assume failure.
  if (!kill_task_)
    OnSessionFailed();

  for (const std::string& record_id : *updated_record_ids) {
    InternalRecord& record = records_[record_id];

    std::vector<uint8_t> serialized_tmpl;
    if (!record.tmpl.Serialize(&serialized_tmpl)) {
      LOG(ERROR) << "Cannot update record " << record_id
                 << " in storage during AuthSession because template "
                    "serialization failed.";
      continue;
    }

    Record current_record(weak_factory_.GetWeakPtr(), record_id);
    if (!WriteRecord(
            current_record, serialized_tmpl.data(), serialized_tmpl.size())) {
      LOG(ERROR) << "Cannot update record " << record_id
                 << " in storage during AuthSession because writing failed.";
    }
  }
}

void FpcBiometricsManager::OnTaskComplete() {
  session_weak_factory_.InvalidateWeakPtrs();
  running_task_ = false;
}

bool FpcBiometricsManager::LoadRecord(const std::string& user_id,
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

  std::vector<uint8_t> tmpl_data_vector(tmpl_data_str.begin(),
                                        tmpl_data_str.end());
  BioTemplate tmpl(bio_lib_->DeserializeTemplate(tmpl_data_vector));
  InternalRecord internal_record = {user_id, label, std::move(tmpl)};
  {
    base::AutoLock guard(records_lock_);
    records_[record_id] = std::move(internal_record);
  }
  LOG(INFO) << "Load record " << record_id << " from disk.";
  return true;
}

bool FpcBiometricsManager::WriteRecord(const BiometricsManager::Record& record,
                                       uint8_t* tmpl_data,
                                       size_t tmpl_size) {
  base::StringPiece tmpl_sp(reinterpret_cast<char*>(tmpl_data), tmpl_size);
  std::string tmpl_base64;
  base::Base64Encode(tmpl_sp, &tmpl_base64);

  return biod_storage_.WriteRecord(
      record, std::make_unique<base::Value>(tmpl_base64));
}

}  // namespace biod
