// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_FAKE_BIOMETRIC_H_
#define BIOD_FAKE_BIOMETRIC_H_

#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>

#include "biod/biometric.h"
#include "biod/fake_biometric_common.h"

namespace biod {

class FakeBiometric : public Biometric, public base::MessageLoopForIO::Watcher {
 public:
  FakeBiometric();

  // base::MessageLoopForIO::Watcher overrides:
  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  // Biometric overrides:
  Biometric::Type GetType() override;
  Biometric::EnrollSession StartEnroll(std::string user_id,
                                       std::string label) override;
  Biometric::AuthenticationSession StartAuthentication() override;
  std::vector<std::unique_ptr<Biometric::Enrollment>> GetEnrollments() override;
  bool DestroyAllEnrollments() override;

  void SetScannedHandler(const Biometric::ScanCallback& on_scan) override;
  void SetAttemptHandler(const Biometric::AttemptCallback& on_attempt) override;
  void SetFailureHandler(const Biometric::FailureCallback& on_failure) override;

 protected:
  void EndEnroll() override;
  void EndAuthentication() override;

 private:
  // This structure stores the enrollment data internally to this class, and is
  // exposed with our Enrollment implementation.
  struct InternalEnrollment {
    std::string user_id;
    std::string label;
  };

  // Our Enrollment implementation is just a proxy for InternalEnrollment, which
  // are all stored inside the FakeBiometric object's enrollments map.
  class Enrollment : public Biometric::Enrollment {
   public:
    Enrollment(const base::WeakPtr<FakeBiometric>& biometric, size_t id)
        : biometric_(biometric), id_(id) {}

    // Biometric::Enrollment overrides:
    uint64_t GetId() const override;
    const std::string& GetUserId() const override;
    const std::string& GetLabel() const override;
    bool SetLabel(std::string label) override;
    bool Remove() override;

   private:
    base::WeakPtr<FakeBiometric> biometric_;
    size_t id_;

    InternalEnrollment* GetInternal() const;
  };

  enum class Mode {
    kNone,
    kEnroll,
    kAuthentication,
  };

  Mode mode_ = Mode::kNone;

  size_t next_enrollment_id_ = 0;
  std::unordered_map<size_t, InternalEnrollment> enrollments_;

  base::ScopedFD fake_input_;
  std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  Biometric::ScanCallback on_scan_;
  Biometric::AttemptCallback on_attempt_;
  Biometric::FailureCallback on_failure_;

  base::WeakPtrFactory<FakeBiometric> session_weak_factory_;
  base::WeakPtrFactory<FakeBiometric> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeBiometric);
};
}  // namespace biod

#endif  // BIOD_FAKE_BIOMETRIC_H_
