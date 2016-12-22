// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_FPC_BIOMETRIC_H_
#define BIOD_FPC_BIOMETRIC_H_

#include "biod/biometric.h"

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

class FpcBiometric : public Biometric {
 public:
  static std::unique_ptr<Biometric> Create();

  // Biometric overrides:
  ~FpcBiometric() override;

  Biometric::Type GetType() override;
  Biometric::EnrollSession StartEnroll(std::string user_id,
                                       std::string label) override;
  Biometric::AuthenticationSession StartAuthentication() override;
  std::vector<std::unique_ptr<Biometric::Enrollment>> GetEnrollments() override;
  bool DestroyAllEnrollments() override;
  void RemoveEnrollmentsFromMemory() override;
  bool ReadEnrollments(
      const std::unordered_set<std::string>& user_ids) override;

  void SetScannedHandler(const Biometric::ScanCallback& on_scan) override;
  void SetAttemptHandler(const Biometric::AttemptCallback& on_attempt) override;
  void SetFailureHandler(const Biometric::FailureCallback& on_failure) override;

 protected:
  void EndEnroll() override;
  void EndAuthentication() override;

 private:
  // Keep this function here because it's used directly by the End* functions.
  void KillSensorTask();

  using TaskRunnerRef = scoped_refptr<base::SingleThreadTaskRunner>;

  class SensorLibrary;

  struct ScanData {
    struct Killed {};
    ScanData() = default;
    explicit ScanData(const Killed&) : killed(true) {}
    explicit ScanData(Biometric::ScanResult r) : success(true), result(r) {}
    explicit ScanData(BioImage i) : success(true), image(std::move(i)) {}

    // True if the scan ended because of kill_task_
    bool killed = false;
    // True if there were NO systemic errors
    bool success = false;
    // Meaningless if success is false, kSuccess on a good scan, user
    // correctable error otherwise
    Biometric::ScanResult result = Biometric::ScanResult::kSuccess;
    // If success and result is kSuccess, this contains the scanned image
    BioImage image;

    explicit operator bool() {
      return !killed && success && result == Biometric::ScanResult::kSuccess &&
             image;
    }
  };

  struct InternalEnrollment {
    std::string user_id;
    std::string label;
    BioTemplate tmpl;
  };

  // Our Enrollment implementation is just a proxy for InternalEnrollment, which
  // are all stored inside the FakeBiometric object's enrollments map.
  class Enrollment : public Biometric::Enrollment {
   public:
    Enrollment(const base::WeakPtr<FpcBiometric>& biometric, std::string id)
        : biometric_(biometric), id_(id) {}

    // Biometric::Enrollment overrides:
    const std::string& GetId() const override;
    const std::string& GetUserId() const override;
    const std::string& GetLabel() const override;
    bool SetLabel(std::string label) override;
    bool Remove() override;

   private:
    base::WeakPtr<FpcBiometric> biometric_;
    std::string id_;

    // These are mutable because the const GetUserId and GetLabel methods use
    // them as storage for the their respective return string refs.
    mutable std::string local_user_id_;
    mutable std::string local_label_;

    // // Only call while holding the enrollments_lock_
    InternalEnrollment* GetInternalLocked() const;

    using EnrollmentIterator =
        std::unordered_map<std::string, InternalEnrollment>::iterator;
    // Used to ensure that the internal enrollment is always handled with proper
    // locks and checking.
    template <typename F>
    bool WithInternal(F f) const {
      if (!biometric_)
        return false;
      base::AutoLock guard(biometric_->enrollments_lock_);
      auto internal_enrollment = biometric_->enrollments_.find(id_);
      if (internal_enrollment == biometric_->enrollments_.end())
        return false;
      f(internal_enrollment);
      return true;
    }
  };

  FpcBiometric();
  bool Init();

  // These are basic wrappers for the client callback functions. We use them for
  // two reasons:
  // - these will always work even if the underlying callbacks are null
  // - the address of these methods never changes, which is important when
  //   posting tasks onto the main thread from the sensor thread.
  void OnScan(Biometric::ScanResult result, bool done);
  void OnAttempt(Biometric::ScanResult result,
                 const std::vector<std::string>& recognized_user_ids);
  void OnFailure();

  // This function are sensor thread only.
  ScanData ScanImage();

  // The following Do*Task and On*Complete functions are meant to be run as a
  // pair using TaskRunner::PostTaskAndReply. The Do*Task functions run in the
  // sensor thread and only read sensor_data_, the dynamically loaded functions,
  // kill_task_, and the sensor itself. The On*Complete functions interpret the
  // result of the task function on the main thread and change the state of this
  // Biometric and its enrollments as needed.

  void DoEnrollTask(const TaskRunnerRef& task_runner,
                    const std::shared_ptr<BioTemplate>& tmpl);
  void OnEnrollComplete(std::string user_id,
                        std::string label,
                        const std::shared_ptr<BioTemplate>& tmpl);

  void DoAuthenticationTask(
      const TaskRunnerRef& task_runner,
      std::shared_ptr<std::unordered_set<std::string>> updated_enrollment_ids);
  void OnAuthenticationComplete(
      std::shared_ptr<std::unordered_set<std::string>> updated_enrollment_ids);

  void OnTaskComplete();

  bool LoadEnrollment(std::string user_id,
                      std::string label,
                      std::string enrollment_id,
                      base::Value* data);
  bool WriteEnrollment(const Biometric::Enrollment& enrollment,
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

  // This lock protects enrollments_.
  base::Lock enrollments_lock_;
  std::unordered_map<std::string, InternalEnrollment> enrollments_;

  // All the following variables are main thread only.

  Biometric::ScanCallback on_scan_;
  Biometric::AttemptCallback on_attempt_;
  Biometric::FailureCallback on_failure_;

  base::WeakPtrFactory<FpcBiometric> session_weak_factory_;
  base::WeakPtrFactory<FpcBiometric> weak_factory_;

  BiodStorage biod_storage_;

  DISALLOW_COPY_AND_ASSIGN(FpcBiometric);
};
}  // namespace biod

#endif  // BIOD_FPC_BIOMETRIC_H_
