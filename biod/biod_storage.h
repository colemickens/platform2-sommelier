// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_BIOD_STORAGE_H_
#define BIOD_BIOD_STORAGE_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/values.h>

#include "biod/biometric.h"
#include "biod/scoped_umask.h"

namespace biod {

class BiodStorage {
 public:
  // Constructor set the file path to be /home/root/<hash of user id>/biod/.
  BiodStorage();

  // Write one enrollment to file in per user stateful. This is called whenever
  // we enroll a new enrollment.
  bool WriteEnrollment(const Biometric::Enrollment& enrollment,
                       std::unique_ptr<base::Value> data);

  // Read all enrollments from file. Uses a file enumerator to enumerate through
  // all enrollment files. Called whenever biod starts or when a new user logs in.
  bool ReadEnrollments(const std::string& user_id,
          const base::Callback<bool(std::string, std::string, std::string,
          base::Value*)>& load_enrollment);

  // Delete one enrollment file. User will be able to do this via UI. True if
  // this enrollment does not exist on disk.
  bool DeleteEnrollment(const std::string& user_id,
                        const std::string& enrollment_id);

  // Generate a uuid with guid.h for each enrollment. Uuid is 128 bit number,
  // which is then turned into a string of format
  // xxxxxxxx_xxxx_xxxx_xxxx_xxxxxxxxxxxx, where x is a lowercase hex number.
  std::string GenerateNewEnrollmentId();

 private:
  base::FilePath root_path_;
};
}  // namespace biod

#endif  // BIOD_BIOD_STORAGE_H_
