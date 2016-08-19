// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIOMETRICS_DAEMON_H_
#define BIOD_BIOMETRICS_DAEMON_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <brillo/dbus/exported_object_manager.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "biod/biometric.h"

namespace biod {

class BiometricWrapper {
 public:
  BiometricWrapper(
      std::unique_ptr<Biometric> biometric,
      brillo::dbus_utils::ExportedObjectManager* object_manager,
      dbus::ObjectPath object_path,
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

  Biometric& get() {
    DCHECK(biometric_);
    return *biometric_.get();
  }

 private:
  class EnrollmentWrapper {
   public:
    EnrollmentWrapper(BiometricWrapper* biometric,
                      std::unique_ptr<Biometric::Enrollment> enrollment,
                      brillo::dbus_utils::ExportedObjectManager* object_manager,
                      const dbus::ObjectPath& object_path);

    const dbus::ObjectPath& path() const { return object_path_; }

   private:
    BiometricWrapper* biometric_;
    std::unique_ptr<Biometric::Enrollment> enrollment_;
    brillo::dbus_utils::DBusObject dbus_object_;
    dbus::ObjectPath object_path_;
    brillo::dbus_utils::ExportedProperty<std::string> property_label_;

    bool SetLabel(brillo::ErrorPtr* error, const std::string& new_label);
    bool Remove(brillo::ErrorPtr* error);

    DISALLOW_COPY_AND_ASSIGN(EnrollmentWrapper);
  };

  std::unique_ptr<Biometric> biometric_;

  brillo::dbus_utils::DBusObject dbus_object_;
  dbus::ObjectPath object_path_;
  brillo::dbus_utils::ExportedProperty<uint32_t> property_type_;
  std::vector<std::unique_ptr<EnrollmentWrapper>> enrollments_;

  Biometric::EnrollSession enroll_;
  dbus::ObjectPath enroll_object_path_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> enroll_dbus_object_;

  Biometric::AuthenticationSession authentication_;
  dbus::ObjectPath authentication_object_path_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> authentication_dbus_object_;

  void OnScanned(Biometric::ScanResult scan_result, bool done);
  void OnAttempt(Biometric::ScanResult scan_result,
                 std::vector<std::string> recognized_user_ids);
  void OnFailure();

  bool StartEnroll(brillo::ErrorPtr* error,
                   const std::string& user_id,
                   const std::string& label,
                   dbus::ObjectPath* enroll_path);
  bool GetEnrollments(brillo::ErrorPtr* error,
                      std::vector<dbus::ObjectPath>* out);
  bool DestroyAllEnrollments(brillo::ErrorPtr* error);
  bool StartAuthentication(brillo::ErrorPtr* error,
                           dbus::ObjectPath* authentication_path);

  bool EnrollCancel(brillo::ErrorPtr* error);

  bool AuthenticationEnd(brillo::ErrorPtr* error);

  void RefreshEnrollmentObjects();

  DISALLOW_COPY_AND_ASSIGN(BiometricWrapper);
};

class BiometricsDaemon {
 public:
  BiometricsDaemon();

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<brillo::dbus_utils::ExportedObjectManager> object_manager_;
  std::vector<std::unique_ptr<BiometricWrapper>> biometrics_;

  DISALLOW_COPY_AND_ASSIGN(BiometricsDaemon);
};
}  // namespace biod

#endif  // BIOD_BIOMETRICS_DAEMON_H_
