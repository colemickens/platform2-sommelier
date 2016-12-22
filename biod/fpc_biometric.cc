// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/fpc_biometric.h"

#include <algorithm>

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

class FpcBiometric::SensorLibrary {
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

FpcBiometric::SensorLibrary::~SensorLibrary() {
  if (needs_close_)
    close_();
}

std::unique_ptr<FpcBiometric::SensorLibrary> FpcBiometric::SensorLibrary::Open(
    const std::shared_ptr<BioLibrary>& bio_lib, int fd) {
  std::unique_ptr<SensorLibrary> lib(new SensorLibrary(bio_lib));
  if (!lib->Init(fd))
    return nullptr;
  return lib;
}

BioEnrollment FpcBiometric::SensorLibrary::BeginEnrollment() {
  return bio_sensor_.BeginEnrollment();
}

std::tuple<int, BioImage> FpcBiometric::SensorLibrary::AcquireImage() {
  std::vector<uint8_t> image_data(image_data_size_);
  int acquire_result = acquire_image_(image_data.data(), image_data.size());
  if (acquire_result)
    return std::tuple<int, BioImage>(acquire_result, BioImage());

  BioImage image = bio_sensor_.CreateImage();
  if (!image || !image.SetData(image_data))
    return std::tuple<int, BioImage>(acquire_result, BioImage());

  return std::tuple<int, BioImage>(0 /* success */, std::move(image));
}

bool FpcBiometric::SensorLibrary::WaitFingerUp() {
  int ret = wait_finger_up_();
  if (ret)
    LOG(ERROR) << "Failed to wait for finger up: " << ret;
  return ret == 0;
}

bool FpcBiometric::SensorLibrary::Cancel() {
  int ret = cancel_();
  if (ret)
    LOG(ERROR) << "Failed to cancel FPC sensor operation: " << ret;
  return ret == 0;
}

bool FpcBiometric::SensorLibrary::Init(int fd) {
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
  LOG(INFO) << "  Pixel Format     : " << (char)(a) << (char)(a >> 8)
            << (char)(a >> 16) << (char)(a >> 24);
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

const std::string& FpcBiometric::Enrollment::GetId() const {
  return id_;
}

const std::string& FpcBiometric::Enrollment::GetUserId() const {
  CHECK(WithInternal([this](EnrollmentIterator i) {
    this->local_user_id_ = i->second.user_id;
  })) << ": Attempted to get user ID for invalid Biometric Enrollment";
  return local_user_id_;
}

const std::string& FpcBiometric::Enrollment::GetLabel() const {
  CHECK(WithInternal([this](EnrollmentIterator i) {
    this->local_label_ = i->second.label;
  })) << ": Attempted to get label for invalid Biometric Enrollment";
  return local_label_;
}

bool FpcBiometric::Enrollment::SetLabel(std::string label) {
  std::string old_label;
  std::vector<uint8_t> serialized_tmpl;
  CHECK(WithInternal([&](EnrollmentIterator i) {
    old_label = i->second.label;
    i->second.label = std::move(label);
    (i->second.tmpl).Serialize(&serialized_tmpl);
  })) << ": Attempted to reset label for invalid Biometric Enrollment";

  if (!biometric_->WriteEnrollment(
          *this, serialized_tmpl.data(), serialized_tmpl.size())) {
    CHECK(WithInternal(
        [&](EnrollmentIterator i) { i->second.label = std::move(old_label); }));
    return false;
  }
  return true;
}

bool FpcBiometric::Enrollment::Remove() {
  if (!biometric_)
    return false;
  if (!biometric_->biod_storage_.DeleteEnrollment(GetUserId(), GetId())) {
    return false;
  }
  return WithInternal(
      [this](EnrollmentIterator i) { biometric_->enrollments_.erase(i); });
}

std::unique_ptr<Biometric> FpcBiometric::Create() {
  std::unique_ptr<FpcBiometric> biometric(new FpcBiometric);
  if (!biometric->Init())
    return nullptr;

  return std::unique_ptr<Biometric>(std::move(biometric));
}

Biometric::Type FpcBiometric::GetType() {
  return Biometric::Type::kFingerprint;
}

Biometric::EnrollSession FpcBiometric::StartEnroll(std::string user_id,
                                                   std::string label) {
  if (running_task_)
    return Biometric::EnrollSession();

  std::shared_ptr<BioTemplate> tmpl = std::make_shared<BioTemplate>();

  kill_task_ = false;
  bool task_will_run = sensor_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FpcBiometric::DoEnrollTask,
                 base::Unretained(this),
                 base::ThreadTaskRunnerHandle::Get(),
                 tmpl),
      base::Bind(&FpcBiometric::OnEnrollComplete,
                 weak_factory_.GetWeakPtr(),
                 std::move(user_id),
                 std::move(label),
                 tmpl));

  if (!task_will_run) {
    LOG(ERROR) << "Failed to schedule enrollment task";
    return Biometric::EnrollSession();
  }

  // Note that the On*Complete function sets running_task_ to false on this
  // thread's message loop, so setting it here does not result in a race
  // condition.
  running_task_ = true;

  return Biometric::EnrollSession(session_weak_factory_.GetWeakPtr());
}

Biometric::AuthenticationSession FpcBiometric::StartAuthentication() {
  if (running_task_)
    return Biometric::AuthenticationSession();

  std::shared_ptr<std::unordered_set<std::string>> updated_enrollment_ids =
      std::make_shared<std::unordered_set<std::string>>();

  kill_task_ = false;
  bool task_will_run = sensor_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&FpcBiometric::DoAuthenticationTask,
                 base::Unretained(this),
                 base::ThreadTaskRunnerHandle::Get(),
                 updated_enrollment_ids),
      base::Bind(&FpcBiometric::OnAuthenticationComplete,
                 weak_factory_.GetWeakPtr(),
                 updated_enrollment_ids));

  if (!task_will_run) {
    LOG(ERROR) << "Failed to schedule authentication task";
    return Biometric::AuthenticationSession();
  }

  // Note that the On*Complete function sets running_task_ to false on this
  // thread's message loop, so setting it here does not result in a race
  // condition.
  running_task_ = true;

  return Biometric::AuthenticationSession(session_weak_factory_.GetWeakPtr());
}

std::vector<std::unique_ptr<Biometric::Enrollment>>
FpcBiometric::GetEnrollments() {
  base::AutoLock guard(enrollments_lock_);
  std::vector<std::unique_ptr<Biometric::Enrollment>> enrollments(
      enrollments_.size());
  std::transform(enrollments_.begin(),
                 enrollments_.end(),
                 enrollments.begin(),
                 [this](decltype(enrollments_)::value_type& enrollment) {
                   return std::unique_ptr<Biometric::Enrollment>(new Enrollment(
                       weak_factory_.GetWeakPtr(), enrollment.first));
                 });
  return enrollments;
}

bool FpcBiometric::DestroyAllEnrollments() {
  base::AutoLock guard(enrollments_lock_);

  // Enumerate through enrollments_ and delete each enrollment.
  bool delete_all_enrollments = true;
  for (auto& enrollment_pair : enrollments_) {
    std::string enrollment_id(enrollment_pair.first);
    std::string user_id(enrollment_pair.second.user_id);
    delete_all_enrollments &=
        biod_storage_.DeleteEnrollment(user_id, enrollment_id);
  }
  enrollments_.clear();
  return delete_all_enrollments;
}

void FpcBiometric::RemoveEnrollmentsFromMemory() {
  base::AutoLock guard(enrollments_lock_);
  enrollments_.clear();
}

bool FpcBiometric::ReadEnrollments(
    const std::unordered_set<std::string>& user_ids) {
  return biod_storage_.ReadEnrollments(user_ids);
}

void FpcBiometric::SetScannedHandler(const Biometric::ScanCallback& on_scan) {
  on_scan_ = on_scan;
}

void FpcBiometric::SetAttemptHandler(
    const Biometric::AttemptCallback& on_attempt) {
  on_attempt_ = on_attempt;
}

void FpcBiometric::SetFailureHandler(
    const Biometric::FailureCallback& on_failure) {
  on_failure_ = on_failure;
}

void FpcBiometric::EndEnroll() {
  KillSensorTask();
}

void FpcBiometric::EndAuthentication() {
  KillSensorTask();
}

void FpcBiometric::KillSensorTask() {
  {
    base::AutoLock guard(kill_task_lock_);
    kill_task_ = true;
  }
  sensor_lib_->Cancel();
}

FpcBiometric::FpcBiometric()
    : sensor_thread_("fpc_sensor"),
      session_weak_factory_(this),
      weak_factory_(this),
      biod_storage_(
          "FpcBiometric",
          base::Bind(&FpcBiometric::LoadEnrollment, base::Unretained(this))) {}

FpcBiometric::~FpcBiometric() {}

bool FpcBiometric::Init() {
  const char kFpcSensorPath[] = "/dev/fpc_sensor0";
  sensor_fd_ = base::ScopedFD(open(kFpcSensorPath, O_RDWR));
  if (sensor_fd_.get() < 0) {
    LOG(ERROR) << "Failed to open " << kFpcSensorPath;
    return false;
  }

  const char kFpcLibName[] = "/opt/fpc/lib/libfp.so";
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

void FpcBiometric::OnScan(Biometric::ScanResult result, bool done) {
  if (!on_scan_.is_null())
    on_scan_.Run(result, done);
}

void FpcBiometric::OnAttempt(
    Biometric::ScanResult result,
    const std::vector<std::string>& recognized_user_ids) {
  if (!on_attempt_.is_null())
    on_attempt_.Run(result, recognized_user_ids);
}

void FpcBiometric::OnFailure() {
  if (!on_failure_.is_null())
    on_failure_.Run();
}

FpcBiometric::ScanData FpcBiometric::ScanImage() {
  DCHECK(sensor_thread_.task_runner()->RunsTasksOnCurrentThread());
  bool success = sensor_lib_->WaitFingerUp();
  {
    base::AutoLock guard(kill_task_lock_);
    if (kill_task_)
      return ScanData(ScanData::Killed());
  }

  if (!success)
    return ScanData();

  int acquire_result;
  BioImage image;
  std::tie(acquire_result, image) = sensor_lib_->AcquireImage();
  {
    base::AutoLock guard(kill_task_lock_);
    if (kill_task_)
      return ScanData(ScanData::Killed());
  }

  switch (acquire_result) {
    case 0:
      break;
    case FP_SENSOR_TOO_FAST:
      return ScanData(Biometric::ScanResult::kTooFast);
    case FP_SENSOR_LOW_IMAGE_QUALITY:
      return ScanData(Biometric::ScanResult::kInsufficient);
    default:
      LOG(ERROR) << "Unexpected result from acquiring image: "
                 << acquire_result;
      return ScanData();
  }

  return ScanData(std::move(image));
}

void FpcBiometric::DoEnrollTask(const TaskRunnerRef& task_runner,
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

    Biometric::ScanResult scan_result = scan.result;
    if (scan) {
      int add_result = enrollment.AddImage(scan.image);
      switch (add_result) {
        case BIO_ENROLLMENT_OK:
          break;
        case BIO_ENROLLMENT_IMMOBILE:
          scan_result = Biometric::ScanResult::kImmobile;
          break;
        case BIO_ENROLLMENT_LOW_COVERAGE:
          scan_result = Biometric::ScanResult::kPartial;
          break;
        case BIO_ENROLLMENT_LOW_QUALITY:
          scan_result = Biometric::ScanResult::kInsufficient;
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
      // Notice we will only ever post the on_scan task on an incomplete
      // enrollment. The complete on_scan task is only posted after the
      // enrollment is added to the enrollments map, which is done only on the
      // main thread's message loop.
      bool task_will_run =
          task_runner->PostTask(FROM_HERE,
                                base::Bind(&FpcBiometric::OnScan,
                                           base::Unretained(this),
                                           scan_result,
                                           false));
      if (!task_will_run) {
        LOG(ERROR) << "Failed to schedule Scan callback";
        return;
      }
    }
  }
}

void FpcBiometric::OnEnrollComplete(std::string user_id,
                                    std::string label,
                                    const std::shared_ptr<BioTemplate>& tmpl) {
  OnTaskComplete();

  if (kill_task_)
    return;

  // Remember tmpl stores a shared pointer to a /pointer/ which contains the
  // actual result.
  if (!*tmpl.get()) {
    OnFailure();
    return;
  }

  std::vector<uint8_t> serialized_tmpl;
  if (!tmpl->Serialize(&serialized_tmpl)) {
    OnFailure();
    return;
  }

  std::string enrollment_id(biod_storage_.GenerateNewEnrollmentId());
  {
    base::AutoLock guard(enrollments_lock_);
    enrollments_.emplace(
        enrollment_id,
        InternalEnrollment{
            std::move(user_id), std::move(label), std::move(*tmpl.get())});
  }
  Enrollment current_enrollment(weak_factory_.GetWeakPtr(), enrollment_id);
  if (!WriteEnrollment(
          current_enrollment, serialized_tmpl.data(), serialized_tmpl.size())) {
    {
      base::AutoLock guard(enrollments_lock_);
      enrollments_.erase(enrollment_id);
    }
    OnFailure();
    return;
  }

  OnScan(Biometric::ScanResult::kSuccess, true);
}

void FpcBiometric::DoAuthenticationTask(
    const TaskRunnerRef& task_runner,
    std::shared_ptr<std::unordered_set<std::string>> updated_enrollment_ids) {
  DCHECK(sensor_thread_.task_runner()->RunsTasksOnCurrentThread());

  if (kill_task_)
    return;

  std::vector<std::string> recognized_user_ids;

  for (;;) {
    ScanData scan = ScanImage();

    // The previous function can return early if this task was killed, or there
    // was an unrecoverable hardware failure.
    if (scan.killed || !scan.success)
      break;

    Biometric::ScanResult result = scan.result;
    if (result == Biometric::ScanResult::kSuccess) {
      recognized_user_ids.clear();

      base::AutoLock guard(enrollments_lock_);
      for (auto& kv : enrollments_) {
        InternalEnrollment& enrollment = kv.second;

        int match_result = enrollment.tmpl.MatchImage(scan.image);
        switch (match_result) {
          case BIO_TEMPLATE_NO_MATCH:
            break;
          case BIO_TEMPLATE_MATCH_UPDATED:  // enrollment.tmpl got updated
            updated_enrollment_ids->insert(kv.first);
          case BIO_TEMPLATE_MATCH:
            recognized_user_ids.emplace_back(enrollment.user_id);
            break;
          case BIO_TEMPLATE_LOW_QUALITY:
            result = Biometric::ScanResult::kInsufficient;
            break;
          case BIO_TEMPLATE_LOW_COVERAGE:
            result = Biometric::ScanResult::kPartial;
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
    if (!recognized_user_ids.empty())
      result = Biometric::ScanResult::kSuccess;

    bool task_will_run =
        task_runner->PostTask(FROM_HERE,
                              base::Bind(&FpcBiometric::OnAttempt,
                                         base::Unretained(this),
                                         result,
                                         std::move(recognized_user_ids)));
    if (!task_will_run) {
      LOG(ERROR) << "Failed to schedule Attempt callback";
      return;
    }
  }
}

void FpcBiometric::OnAuthenticationComplete(
    std::shared_ptr<std::unordered_set<std::string>> updated_enrollment_ids) {
  OnTaskComplete();

  // Authentication never ends except on error or being killed. If no kill
  // signal was given, we can assume failure.
  if (!kill_task_)
    OnFailure();

  for (const std::string& enrollment_id : *updated_enrollment_ids) {
    InternalEnrollment& enrollment = enrollments_[enrollment_id];

    std::vector<uint8_t> serialized_tmpl;
    if (!enrollment.tmpl.Serialize(&serialized_tmpl)) {
      LOG(ERROR) << "Cannot update enrollment " << enrollment_id
                 << " in storage during authentication because template "
                    "serialization failed.";
      continue;
    }

    Enrollment current_enrollment(weak_factory_.GetWeakPtr(), enrollment_id);
    if (!WriteEnrollment(current_enrollment,
                         serialized_tmpl.data(),
                         serialized_tmpl.size())) {
      LOG(ERROR) << "Cannot update enrollment " << enrollment_id
                 << " in storage during authentication because writing failed.";
    }
  }
}

void FpcBiometric::OnTaskComplete() {
  session_weak_factory_.InvalidateWeakPtrs();
  running_task_ = false;
}

bool FpcBiometric::LoadEnrollment(std::string user_id,
                                  std::string label,
                                  std::string enrollment_id,
                                  base::Value* data) {
  std::string tmpl_data_base64;
  if (!data->GetAsString(&tmpl_data_base64)) {
    LOG(ERROR) << "Cannot load data string from enrollment " << enrollment_id
               << ".";
    return false;
  }

  base::StringPiece tmpl_data_base64_sp(tmpl_data_base64);
  std::string tmpl_data_str;
  base::Base64Decode(tmpl_data_base64_sp, &tmpl_data_str);

  std::vector<uint8_t> tmpl_data_vector(tmpl_data_str.begin(),
                                        tmpl_data_str.end());
  BioTemplate tmpl(bio_lib_->DeserializeTemplate(tmpl_data_vector));
  InternalEnrollment internal_enrollment = {
      std::move(user_id), std::move(label), std::move(tmpl)};
  {
    base::AutoLock guard(enrollments_lock_);
    enrollments_[enrollment_id] = std::move(internal_enrollment);
  }
  LOG(INFO) << "Load enrollment " << enrollment_id << " from disk.";
  return true;
}

bool FpcBiometric::WriteEnrollment(const Biometric::Enrollment& enrollment,
                                   uint8_t* tmpl_data,
                                   size_t tmpl_size) {
  base::StringPiece tmpl_sp(reinterpret_cast<char*>(tmpl_data), tmpl_size);
  std::string tmpl_base64;
  base::Base64Encode(tmpl_sp, &tmpl_base64);

  return biod_storage_.WriteEnrollment(
      enrollment,
      std::unique_ptr<base::Value>(new base::StringValue(tmpl_base64)));
}

}  // namespace biod
