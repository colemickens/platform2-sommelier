// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_grpc_service.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>

namespace diagnostics {

namespace {

// Folder path exposed by sysfs EC driver.
constexpr char kEcDriverSysfsPath[] = "sys/bus/platform/devices/GOOG000C:00/";

// Folder path to EC properties exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
constexpr char kEcDriverSysfsPropertiesPath[] = "properties/";

// EC property |global_mic_mute_led|.
constexpr char kEcPropertyGlobalMicMuteLed[] = "global_mic_mute_led";

// Makes a dump of the specified file. Returns whether the dumping succeeded.
bool MakeFileDump(const base::FilePath& file_path,
                  grpc_api::FileDump* file_dump) {
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    VPLOG(2) << "Failed to read from " << file_path.value();
    return false;
  }
  const base::FilePath canonical_file_path =
      base::MakeAbsoluteFilePath(file_path);
  if (canonical_file_path.empty()) {
    PLOG(ERROR) << "Failed to obtain canonical path for " << file_path.value();
    return false;
  }
  VLOG(2) << "Read " << file_contents.size() << " bytes from "
          << file_path.value() << " with canonical path "
          << canonical_file_path.value();
  file_dump->set_path(file_path.value());
  file_dump->set_canonical_path(canonical_file_path.value());
  file_dump->set_contents(std::move(file_contents));
  return true;
}

}  // namespace

DiagnosticsdGrpcService::DiagnosticsdGrpcService(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DiagnosticsdGrpcService::~DiagnosticsdGrpcService() = default;

void DiagnosticsdGrpcService::SendMessageToUi(
    std::unique_ptr<grpc_api::SendMessageToUiRequest> request,
    const SendMessageToUiCallback& callback) {
  NOTIMPLEMENTED();
}

void DiagnosticsdGrpcService::GetProcData(
    std::unique_ptr<grpc_api::GetProcDataRequest> request,
    const GetProcDataCallback& callback) {
  DCHECK(request);
  auto reply = std::make_unique<grpc_api::GetProcDataResponse>();
  switch (request->type()) {
    case grpc_api::GetProcDataRequest::FILE_UPTIME:
      AddFileDump(base::FilePath("proc/uptime"), reply->mutable_file_dump());
      break;
    case grpc_api::GetProcDataRequest::FILE_MEMINFO:
      AddFileDump(base::FilePath("proc/meminfo"), reply->mutable_file_dump());
      break;
    case grpc_api::GetProcDataRequest::FILE_LOADAVG:
      AddFileDump(base::FilePath("proc/loadavg"), reply->mutable_file_dump());
      break;
    case grpc_api::GetProcDataRequest::FILE_STAT:
      AddFileDump(base::FilePath("proc/stat"), reply->mutable_file_dump());
      break;
    case grpc_api::GetProcDataRequest::FILE_NET_NETSTAT:
      AddFileDump(base::FilePath("proc/net/netstat"),
                  reply->mutable_file_dump());
      break;
    case grpc_api::GetProcDataRequest::FILE_NET_DEV:
      AddFileDump(base::FilePath("proc/net/dev"), reply->mutable_file_dump());
      break;
    default:
      LOG(ERROR) << "GetProcData gRPC request type unset or invalid: "
                 << request->type();
      // Error is designated by a reply with the empty list of entries.
      callback.Run(std::move(reply));
      return;
  }
  VLOG(1) << "Completing GetProcData gRPC request of type " << request->type()
          << ", returning " << reply->file_dump_size() << " items";
  callback.Run(std::move(reply));
}

void DiagnosticsdGrpcService::GetEcProperty(
    std::unique_ptr<grpc_api::GetEcPropertyRequest> request,
    const GetEcPropertyCallback& callback) {
  auto reply = std::make_unique<grpc_api::GetEcPropertyResponse>();
  base::FilePath property_file_path;

  DCHECK(request);
  switch (request->property()) {
    case grpc_api::GetEcPropertyRequest::PROPERTY_GLOBAL_MIC_MUTE_LED:
      property_file_path = base::FilePath(kEcPropertyGlobalMicMuteLed);
      break;
    default:
      LOG(ERROR) << "GetEcProperty gRPC request property is invalid or unset: "
                 << request->property();
      reply->set_status(
          grpc_api::GetEcPropertyResponse::STATUS_ERROR_REQUIRED_FIELD_MISSING);
      callback.Run(std::move(reply));
      return;
  }

  DCHECK(!property_file_path.empty());
  base::FilePath sysfs_file_path = root_dir_.Append(kEcDriverSysfsPath)
                                       .Append(kEcDriverSysfsPropertiesPath)
                                       .Append(property_file_path);
  // Assign |file_content| to |reply->payload| only in case of successfull file
  // reading. If case of failed reading |reply->payload| should to be empty.
  //
  // |base::ReadFileToString| can store part of file in |file_content| until
  // I/O error was occured.
  std::string file_content;
  if (base::ReadFileToString(sysfs_file_path, &file_content)) {
    reply->set_status(grpc_api::GetEcPropertyResponse::STATUS_OK);
    reply->set_payload(std::move(file_content));
  } else {
    VPLOG(2) << "Sysfs file " << sysfs_file_path.value() << " read error";
    reply->set_status(
        grpc_api::GetEcPropertyResponse::STATUS_ERROR_ACCESSING_DRIVER);
  }
  callback.Run(std::move(reply));
}

void DiagnosticsdGrpcService::AddFileDump(
    const base::FilePath& relative_file_path,
    google::protobuf::RepeatedPtrField<grpc_api::FileDump>* file_dumps) {
  DCHECK(!relative_file_path.IsAbsolute());
  grpc_api::FileDump file_dump;
  if (!MakeFileDump(root_dir_.Append(relative_file_path), &file_dump)) {
    // When a file is failed to be dumped, it's just omitted from the returned
    // list of entries.
    return;
  }
  file_dumps->Add()->Swap(&file_dump);
}

}  // namespace diagnostics
