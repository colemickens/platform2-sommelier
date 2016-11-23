// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIOMETRIC_H_
#define BIOD_BIOMETRIC_H_

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>

namespace biod {

// A Biometric object represents one biometric input device and all of the
// enrollments registered with it. At a high level, there are 3 operations that
// are supported: 1) enrolling new enrollment objects, 2) authenticating
// against those enrollment objects, and 3) destroying all enrollment
// objects made from this biometrics. For DestroyAllEnrollments the operation
// is as simple as calling the function. For the other operations,
// the biometric object must be entered into authentication or enroll mode,
// which is represented in code by the return of the session objects.
// Enroll and authentication can be thought of as session objects that
// are ongoing as long as the unique pointers remain in scope and
// the End/Cancel methods haven't been called. It's undefined what
// StartEnroll or StartAuthentication will do if there is an valid
// outstanding Enroll or Authentication object in the wild.
class Biometric {
 public:
  enum class Type {
    kFingerprint = 0,
    kRetina = 1,
    kFace = 2,
    kVoice = 3,
  };

  // Results of any type of any scan operation can fail due to user error. These
  // codes tell the user a little bit about what they did wrong, so they should
  // be conveyed to the user somehow after unsuccessful scan attempts.
  enum class ScanResult {
    kSuccess = 0,
    kPartial = 1,
    kInsufficient = 2,
    kSensorDirty = 3,
    kTooSlow = 4,
    kTooFast = 5,
    kImmobile = 6,
  };

  struct EnrollEnder {
    void operator()(Biometric* biometric) { biometric->EndEnroll(); }
  };

  struct AuthenticationEnder {
    void operator()(Biometric* biometric) { biometric->EndAuthentication(); }
  };

  // Invokes the function object F with a given Biometric object when this
  // Session(Enroll or Authentication) object goes out of scope.
  // It's possible that this will do nothing in the case that the session
  // has ended due to failure/finishing or the Biometric object is
  // no longer valid.
  template <typename F>
  class Session {
   public:
    Session() = default;

    Session(Session<F>&& rhs) : biometric_(rhs.biometric_) {
      rhs.biometric_.reset();
    }

    explicit Session(const base::WeakPtr<Biometric>& biometric)
        : biometric_(biometric) {}

    ~Session() { End(); }

    Session<F>& operator=(Session<F>&& rhs) {
      End();
      biometric_ = rhs.biometric_;
      rhs.biometric_.reset();
      return *this;
    }

    explicit operator bool() const { return biometric_; }

    // Has the same effect of letting this object go out of scope, but allows
    // one to reuse the storage of this object.
    void End() {
      if (biometric_) {
        F f;
        f(biometric_.get());
        biometric_.reset();
      }
    }

   private:
    base::WeakPtr<Biometric> biometric_;
  };

  // Returned by StartEnroll to ensure that enrollment eventually ends.
  using EnrollSession = Session<EnrollEnder>;

  // Returned by StartAuthentication to ensure that authentication eventually
  // ends.
  using AuthenticationSession = Session<AuthenticationEnder>;

  // Represents an enrollment previously registered with this biometric in an
  // enroll session. These objects can be retrieved with GetEnrollments.
  class Enrollment {
   public:
    virtual ~Enrollment() {}
    virtual const std::string& GetId() const = 0;
    virtual const std::string& GetUserId() const = 0;
    virtual const std::string& GetLabel() const = 0;

    // Returns true on success.
    virtual bool SetLabel(std::string label) = 0;

    // Returns true on success.
    virtual bool Remove() = 0;
  };

  virtual ~Biometric() {}
  virtual Type GetType() = 0;

  // Puts this biometric into enroll mode, which can be ended by letting the
  // returned session fall out of scope. The user_id is arbitrary and is given
  // to attempt callbacks in the Authentication object. The label should be
  // human readable and ideally from the user themselves. The label can be read
  // and modified from the Enrollment objects. This will fail if ANY other mode
  // is active. Returns a false EnrollSession on failure.
  virtual EnrollSession StartEnroll(std::string user_id, std::string label) = 0;

  // Puts this biometric into authentication mode, which can be ended by letting
  // the returned session fall out of scope. This will fail if ANY other mode is
  // active. Returns a false AuthenticationSession on failure.
  virtual AuthenticationSession StartAuthentication() = 0;

  // Gets the enrollments registered with this biometric. Some enrollments will
  // naturally be unaccessible because they are currently in an encrypted state,
  // so those will silently be left out of the returned vector.
  virtual std::vector<std::unique_ptr<Enrollment>> GetEnrollments() = 0;

  // Irreversibly destroys enrollments registered with this biometric, including
  // currently encrypted ones. Returns true if successful.
  virtual bool DestroyAllEnrollments() = 0;

  // Remove all enrollments from memory. Still keep them in storage.
  virtual void RemoveEnrollmentsFromMemory() = 0;

  // Read all the enrollments for each of the users in the set. Return true if
  // successful.
  virtual bool ReadEnrollments(
      const std::unordered_set<std::string>& user_ids) = 0;

  // The callbacks should remain valid as long as this object is valid.

  // Invoked from Enroll mode whenever the user attempts a scan. The first
  // parameter tells whether the scan was successful. If it was successful, the
  // second parameter MAY be true to indicate that the enrollment was complete
  // and is now over. However, it make take many successful scans before this is
  // true. When the enrollment is complete, enroll mode will automatically be
  // ended.
  using ScanCallback = base::Callback<void(ScanResult, bool done)>;
  virtual void SetScannedHandler(const ScanCallback& on_scan) = 0;

  // Invoked from authentication mode to indicate either a bad scan of any kind,
  // or a successful scan. In the case of successful scan, recognized_user_ids
  // shall be a (possibly zero-length) array of strings that are equal to all
  // enrollments user IDs that match the scan.
  using AttemptCallback = base::Callback<void(
      ScanResult, std::vector<std::string> recognized_user_ids)>;
  virtual void SetAttemptHandler(const AttemptCallback& on_attempt) = 0;

  // Invoked during any mode to indicate that the mode as ended with failure.
  // Any enrollment that was underway is thrown away and authentication will no
  // longer be happening.
  using FailureCallback = base::Callback<void()>;
  virtual void SetFailureHandler(const FailureCallback& on_failure) = 0;

 protected:
  virtual void EndEnroll() = 0;
  virtual void EndAuthentication() = 0;
};
}  // namespace biod

#endif  // BIOD_BIOMETRIC_H_
