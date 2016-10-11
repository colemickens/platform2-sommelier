// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/biod_storage.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>
#include <base/guid.h>
#include <base/json/json_string_value_serializer.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_util.h>
#include <base/values.h>
#include <brillo/cryptohome.h>

namespace biod {

using base::FilePath;

namespace {
const char kRootPath[] = "/home/root";
const char kEnrollmentFileName[] = "biod/Enrollment";
const char kBiod[] = "biod";
const char kUserId[] = "user_id";
const char kLabel[] = "label";
const char kEnrollmentId[] = "enrollment_id";
const char kData[] = "data";
}

BiodStorage::BiodStorage(): root_path_(kRootPath) {}

bool BiodStorage::WriteEnrollment(const Biometric::Enrollment& enrollment,
                                  std::unique_ptr<base::Value> data) {
  const std::string& user_id(enrollment.GetUserId());
  const std::string& enrollment_id(enrollment.GetId());
  base::DictionaryValue enrollment_value;
  enrollment_value.SetString(kUserId, user_id);
  enrollment_value.SetString(kLabel, enrollment.GetLabel());
  enrollment_value.SetString(kEnrollmentId, enrollment_id);
  enrollment_value.Set(kData, std::move(data));

  std::string json_string;
  JSONStringValueSerializer json_serializer(&json_string);
  if (!json_serializer.Serialize(enrollment_value)) {
    LOG(ERROR) << "Failed to serialize enrollment with id " << enrollment_id
               << " to JSON.";
    return false;
  }

  std::unique_ptr<ScopedUmask> owner_only_umask(new ScopedUmask(~(0700)));

  FilePath enrollment_storage_filename =
      root_path_.Append(user_id)
          .Append(kBiod)
          .Append(biometric_path_)
          .Append(kEnrollmentFileName + enrollment_id);
  if (!base::CreateDirectory(enrollment_storage_filename.DirName())) {
    LOG(ERROR) << "Cannot create directory: "
               << enrollment_storage_filename.DirName().value() << ".";
    return false;
  }

  owner_only_umask.reset(new ScopedUmask(~(0600)));

  if (!base::ImportantFileWriter::WriteFileAtomically(
          enrollment_storage_filename, json_string)) {
    LOG(ERROR) << "Failed to write JSON file: "
               << enrollment_storage_filename.value() << ".";
    return false;
  }
  LOG(INFO) << "Done writing enrollment with id " << enrollment_id
            << " to file successfully. ";
  return true;
}

bool BiodStorage::ReadEnrollments(const std::string& user_id,
          const base::Callback<bool(std::string, std::string, std::string,
          base::Value*)>& load_enrollment) {
  FilePath biod_path =
      root_path_.Append(user_id).Append(kBiod).Append(biometric_path_);
  base::FileEnumerator enum_enrollments(biod_path,
                                        false,
                                        base::FileEnumerator::FILES,
                                        FILE_PATH_LITERAL("Enrollment*"));
  bool read_all_enrollments_successfully = true;
  for (FilePath enrollment_path = enum_enrollments.Next();
       !enrollment_path.empty();
       enrollment_path = enum_enrollments.Next()) {
    std::string json_string;
    if (!base::ReadFileToString(enrollment_path, &json_string)) {
      LOG(ERROR) << "Failed to read the string from " << enrollment_path.value()
                 << ".";
      read_all_enrollments_successfully = false;
      continue;
    }

    JSONStringValueDeserializer json_deserializer(json_string);
    json_deserializer.set_allow_trailing_comma(true);
    int error_code;
    std::string error_message;
    std::unique_ptr<base::Value> enrollment_value(
        json_deserializer.Deserialize(&error_code, &error_message));

    if (!enrollment_value) {
      LOG_IF(ERROR, error_code) << "Error in deserializing JSON from path "
                                << enrollment_path.value() << " with code "
                                << error_code << ".";
      LOG_IF(ERROR, !error_message.empty())
          << "JSON error message: " << error_message << ".";
      read_all_enrollments_successfully = false;
      continue;
    }

    base::DictionaryValue* enrollment_dictionary;

    if (!enrollment_value->GetAsDictionary(&enrollment_dictionary)) {
      LOG(ERROR) << "Cannot cast " << enrollment_path.value()
                 << " to a dictionary value.";
      read_all_enrollments_successfully = false;
      continue;
    }

    std::string user_id;

    if (!enrollment_dictionary->GetString(kUserId, &user_id)) {
      LOG(ERROR) << "Cannot read user id from " << enrollment_path.value()
                 << ".";
      read_all_enrollments_successfully = false;
      continue;
    }

    std::string label;

    if (!enrollment_dictionary->GetString(kLabel, &label)) {
      LOG(ERROR) << "Cannot read label from " << enrollment_path.value() << ".";
      read_all_enrollments_successfully = false;
      continue;
    }

    std::string enrollment_id;

    if (!(enrollment_dictionary->GetString(kEnrollmentId, &enrollment_id))) {
      LOG(ERROR) << "Cannot read enrollment id from " << enrollment_path.value()
                 << ".";
      read_all_enrollments_successfully = false;
      continue;
    }

    base::Value* data;

    if (!(enrollment_dictionary->Get(kData, &data))) {
      LOG(ERROR) << "Cannot read data from " << enrollment_path.value() << ".";
      read_all_enrollments_successfully = false;
      continue;
    }

    if (!load_enrollment.Run(user_id, label, enrollment_id, data)) {
      LOG(ERROR) << "Cannot load enrollment from " << enrollment_path.value()
                 << ".";
      read_all_enrollments_successfully = false;
      continue;
    }
  }
  return read_all_enrollments_successfully;
}

bool BiodStorage::DeleteEnrollment(const std::string& user_id,
                                   const std::string& enrollment_id) {
  FilePath enrollment_storage_filename =
      root_path_.Append(user_id)
          .Append(kBiod)
          .Append(biometric_path_)
          .Append(kEnrollmentFileName + enrollment_id);

  if (!base::PathExists(enrollment_storage_filename)) {
    LOG(INFO) << "Trying to delete enrollment " << enrollment_id
              << " which does not exist on disk.";
    return true;
  }
  if (!base::DeleteFile(enrollment_storage_filename, false)) {
    LOG(ERROR) << "Fail to delete enrollment " << enrollment_id
               << " from disk.";
    return false;
  }
  LOG(INFO) << "Done deleting enrollment " << enrollment_id << " from disk.";
  return true;
}

std::string BiodStorage::GenerateNewEnrollmentId() {
  std::string enrollment_id(base::GenerateGUID());
  // dbus member names only allow '_'
  std::replace(enrollment_id.begin(), enrollment_id.end(), '-', '_');
  return enrollment_id;
}
}  // namespace biod
