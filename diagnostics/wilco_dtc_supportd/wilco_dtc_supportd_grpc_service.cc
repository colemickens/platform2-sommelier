// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_grpc_service.h"

#include <cstdint>
#include <iterator>
#include <utility>

#include <base/bind.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/sys_info.h>

#include "diagnostics/wilco_dtc_supportd/ec_constants.h"
#include "diagnostics/wilco_dtc_supportd/vpd_constants.h"

namespace diagnostics {

// The total size of "string" and "bytes" fields in one
// PerformWebRequestParameter must not exceed 1MB.
const int kMaxPerformWebRequestParameterSizeInBytes = 1000 * 1000;

// The maximum number of header in PerformWebRequestParameter.
const int kMaxNumberOfHeadersInPerformWebRequestParameter = 1000 * 1000;

namespace {

using PerformWebRequestResponseCallback =
    WilcoDtcSupportdGrpcService::PerformWebRequestResponseCallback;
using DelegateWebRequestStatus =
    WilcoDtcSupportdGrpcService::Delegate::WebRequestStatus;
using DelegateWebRequestHttpMethod =
    WilcoDtcSupportdGrpcService::Delegate::WebRequestHttpMethod;
using GetAvailableRoutinesCallback =
    WilcoDtcSupportdGrpcService::GetAvailableRoutinesCallback;
using RunRoutineCallback = WilcoDtcSupportdGrpcService::RunRoutineCallback;
using GetRoutineUpdateCallback =
    WilcoDtcSupportdGrpcService::GetRoutineUpdateCallback;
using GetConfigurationDataCallback =
    WilcoDtcSupportdGrpcService::GetConfigurationDataCallback;

// Https prefix expected to be a prefix of URL in PerformWebRequestParameter.
constexpr char kHttpsPrefix[] = "https://";

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

// Calculates the size of all "string" and "bytes" fields in the request.
// Must be updated if grpc_api::PerformWebRequestParameter proto is updated.
int64_t CalculateWebRequestParameterSize(
    const std::unique_ptr<grpc_api::PerformWebRequestParameter>& parameter) {
  int64_t size = parameter->url().length() + parameter->request_body().size();
  for (const std::string& header : parameter->headers()) {
    size += header.length();
  }
  return size;
}

// Forwards and wraps status & HTTP status into gRPC PerformWebRequestResponse.
void ForwardWebGrpcResponse(const PerformWebRequestResponseCallback& callback,
                            DelegateWebRequestStatus status,
                            int http_status,
                            base::StringPiece response_body) {
  auto reply = std::make_unique<grpc_api::PerformWebRequestResponse>();
  switch (status) {
    case DelegateWebRequestStatus::kOk:
      reply->set_status(grpc_api::PerformWebRequestResponse::STATUS_OK);
      reply->set_http_status(http_status);
      reply->set_response_body(response_body.as_string());
      break;
    case DelegateWebRequestStatus::kNetworkError:
      reply->set_status(
          grpc_api::PerformWebRequestResponse::STATUS_NETWORK_ERROR);
      break;
    case DelegateWebRequestStatus::kHttpError:
      reply->set_status(grpc_api::PerformWebRequestResponse::STATUS_HTTP_ERROR);
      reply->set_http_status(http_status);
      reply->set_response_body(response_body.as_string());
      break;
    case DelegateWebRequestStatus::kInternalError:
      reply->set_status(
          grpc_api::PerformWebRequestResponse::STATUS_INTERNAL_ERROR);
      break;
  }
  callback.Run(std::move(reply));
}

// Converts gRPC HTTP method into WilcoDtcSupportdGrpcService::Delegate's HTTP
// method, returns false if HTTP method is invalid.
bool GetDelegateWebRequestHttpMethod(
    grpc_api::PerformWebRequestParameter::HttpMethod http_method,
    DelegateWebRequestHttpMethod* delegate_http_method) {
  switch (http_method) {
    case grpc_api::PerformWebRequestParameter::HTTP_METHOD_GET:
      *delegate_http_method = DelegateWebRequestHttpMethod::kGet;
      return true;
    case grpc_api::PerformWebRequestParameter::HTTP_METHOD_HEAD:
      *delegate_http_method = DelegateWebRequestHttpMethod::kHead;
      return true;
    case grpc_api::PerformWebRequestParameter::HTTP_METHOD_POST:
      *delegate_http_method = DelegateWebRequestHttpMethod::kPost;
      return true;
    case grpc_api::PerformWebRequestParameter::HTTP_METHOD_PUT:
      *delegate_http_method = DelegateWebRequestHttpMethod::kPut;
      return true;
    default:
      LOG(ERROR) << "The HTTP method is unset or invalid: "
                 << static_cast<int>(http_method);
      return false;
  }
}

// Forwards and wraps available routines into a gRPC response.
void ForwardGetAvailableRoutinesResponse(
    const GetAvailableRoutinesCallback& callback,
    const std::vector<grpc_api::DiagnosticRoutine>& routines) {
  auto reply = std::make_unique<grpc_api::GetAvailableRoutinesResponse>();
  for (auto routine : routines)
    reply->add_routines(routine);
  callback.Run(std::move(reply));
}

// Forwards and wraps the result of a RunRoutine command into a gRPC response.
void ForwardRunRoutineResponse(const RunRoutineCallback& callback,
                               int uuid,
                               grpc_api::DiagnosticRoutineStatus status) {
  auto reply = std::make_unique<grpc_api::RunRoutineResponse>();
  reply->set_uuid(uuid);
  reply->set_status(status);
  callback.Run(std::move(reply));
}

// Forwards and wraps the results of a GetRoutineUpdate command into a gRPC
// response.
void ForwardGetRoutineUpdateResponse(
    const GetRoutineUpdateCallback& callback,
    int uuid,
    grpc_api::DiagnosticRoutineStatus status,
    int progress_percent,
    grpc_api::DiagnosticRoutineUserMessage user_message,
    const std::string& output,
    const std::string& status_message) {
  auto reply = std::make_unique<grpc_api::GetRoutineUpdateResponse>();
  reply->set_uuid(uuid);
  reply->set_status(status);
  reply->set_progress_percent(progress_percent);
  reply->set_user_message(user_message);
  reply->set_output(output);
  reply->set_status_message(status_message);
  callback.Run(std::move(reply));
}

// Forwards and wraps the result of a GetConfigurationDataFromBrowser into gRPC
// response.
void ForwardGetConfigurationDataResponse(
    const GetConfigurationDataCallback& callback,
    const std::string& json_configuration_data) {
  auto reply = std::make_unique<grpc_api::GetConfigurationDataResponse>();
  reply->set_json_configuration_data(json_configuration_data);
  callback.Run(std::move(reply));
}

// Converts gRPC GetEcPropertyRequest::Property to property path.
//
// Returns |nullptr| if |property| is invlid or unset.
const char* GetEcPropertyPath(
    grpc_api::GetEcPropertyRequest::Property property) {
  switch (property) {
    case grpc_api::GetEcPropertyRequest::PROPERTY_GLOBAL_MIC_MUTE_LED:
      return kEcPropertyGlobalMicMuteLed;
    case grpc_api::GetEcPropertyRequest::PROPERTY_FN_LOCK:
      return kEcPropertyFnLock;
    case grpc_api::GetEcPropertyRequest::PROPERTY_NIC:
      return kEcPropertyNic;
    case grpc_api::GetEcPropertyRequest::PROPERTY_EXT_USB_PORT_EN:
      return kEcPropertyExtUsbPortEn;
    case grpc_api::GetEcPropertyRequest::PROPERTY_WIRELESS_SW_WLAN:
      return kEcPropertyWirelessSwWlan;
    case grpc_api::GetEcPropertyRequest::
        PROPERTY_AUTO_BOOT_ON_TRINITY_DOCK_ATTACH:
      return kEcPropertyAutoBootOnTrinityDockAttach;
    case grpc_api::GetEcPropertyRequest::PROPERTY_ICH_AZALIA_EN:
      return kEcPropertyIchAzaliaEn;
    case grpc_api::GetEcPropertyRequest::PROPERTY_SIGN_OF_LIFE_KBBL:
      return kEcPropertySignOfLifeKbbl;
    default:
      return nullptr;
  }
}

// While dumping files in a directory, determines if we should follow a symlink
// or not. Currently, we only follow symlinks one level down from /sys/class/*/.
// For example, we would follow a symlink from /sys/class/hwmon/hwmon0, but we
// would not follow a symlink from /sys/class/hwmon/hwmon0/device.
bool ShouldFollowSymlink(const base::FilePath& link, base::FilePath root_dir) {
  // Path relative to the root directory where we will follow symlinks.
  constexpr char kAllowableSymlinkParentDir[] = "sys/class";
  return base::FilePath(root_dir.Append(kAllowableSymlinkParentDir)) ==
         link.DirName().DirName();
}

}  // namespace

WilcoDtcSupportdGrpcService::WilcoDtcSupportdGrpcService(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

WilcoDtcSupportdGrpcService::~WilcoDtcSupportdGrpcService() = default;

void WilcoDtcSupportdGrpcService::SendMessageToUi(
    std::unique_ptr<grpc_api::SendMessageToUiRequest> request,
    const SendMessageToUiCallback& callback) {
  NOTIMPLEMENTED();
}

void WilcoDtcSupportdGrpcService::GetProcData(
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

void WilcoDtcSupportdGrpcService::GetSysfsData(
    std::unique_ptr<grpc_api::GetSysfsDataRequest> request,
    const GetSysfsDataCallback& callback) {
  DCHECK(request);
  auto reply = std::make_unique<grpc_api::GetSysfsDataResponse>();
  switch (request->type()) {
    case grpc_api::GetSysfsDataRequest::CLASS_HWMON:
      AddDirectoryDump(base::FilePath("sys/class/hwmon/"),
                       reply->mutable_file_dump());
      break;
    case grpc_api::GetSysfsDataRequest::CLASS_THERMAL:
      AddDirectoryDump(base::FilePath("sys/class/thermal/"),
                       reply->mutable_file_dump());
      break;
    case grpc_api::GetSysfsDataRequest::FIRMWARE_DMI_TABLES:
      AddDirectoryDump(base::FilePath("sys/firmware/dmi/tables/"),
                       reply->mutable_file_dump());
      break;
    default:
      LOG(ERROR) << "GetSysfsData gRPC request type unset or invalid: "
                 << request->type();
      // Error is designated by a reply with the empty list of entries.
      callback.Run(std::move(reply));
      return;
  }
  VLOG(1) << "Completing GetSysfsData gRPC request of type " << request->type()
          << ", returning " << reply->file_dump_size() << " items";
  callback.Run(std::move(reply));
}

void WilcoDtcSupportdGrpcService::GetEcTelemetry(
    std::unique_ptr<grpc_api::GetEcTelemetryRequest> request,
    const GetEcTelemetryCallback& callback) {
  DCHECK(request);
  auto reply = std::make_unique<grpc_api::GetEcTelemetryResponse>();
  if (request->payload().empty()) {
    LOG(ERROR) << "GetEcTelemetry gRPC request payload is empty";
    reply->set_status(
        grpc_api::GetEcTelemetryResponse::STATUS_ERROR_INPUT_PAYLOAD_EMPTY);
    callback.Run(std::move(reply));
    return;
  }
  if (request->payload().length() > kEcGetTelemetryPayloadMaxSize) {
    LOG(ERROR) << "GetEcTelemetry gRPC request payload size is exceeded: "
               << request->payload().length() << " vs "
               << kEcGetTelemetryPayloadMaxSize << " allowed";
    reply->set_status(grpc_api::GetEcTelemetryResponse::
                          STATUS_ERROR_INPUT_PAYLOAD_MAX_SIZE_EXCEEDED);
    callback.Run(std::move(reply));
    return;
  }

  base::FilePath telemetry_file_path =
      root_dir_.Append(kEcGetTelemetryFilePath);

  int write_result =
      base::WriteFile(telemetry_file_path, request->payload().c_str(),
                      request->payload().length());
  if (write_result != request->payload().length()) {
    VPLOG(2) << "GetEcTelemetry gRPC can not write request payload to the "
             << "telemetry node: " << telemetry_file_path.value();
    reply->set_status(
        grpc_api::GetEcTelemetryResponse::STATUS_ERROR_ACCESSING_DRIVER);
    callback.Run(std::move(reply));
    return;
  }

  // Reply payload must be empty in case of any failure.
  std::string file_content;
  if (base::ReadFileToString(telemetry_file_path, &file_content)) {
    reply->set_status(grpc_api::GetEcTelemetryResponse::STATUS_OK);
    reply->set_payload(std::move(file_content));
  } else {
    VPLOG(2) << "GetEcTelemetry gRPC can not read EC telemetry command "
             << "response from telemetry node: " << telemetry_file_path.value();
    reply->set_status(
        grpc_api::GetEcTelemetryResponse::STATUS_ERROR_ACCESSING_DRIVER);
  }
  callback.Run(std::move(reply));
}

void WilcoDtcSupportdGrpcService::GetEcProperty(
    std::unique_ptr<grpc_api::GetEcPropertyRequest> request,
    const GetEcPropertyCallback& callback) {
  DCHECK(request);

  auto reply = std::make_unique<grpc_api::GetEcPropertyResponse>();

  const char* property_file_path = GetEcPropertyPath(request->property());
  if (!property_file_path) {
    LOG(ERROR) << "GetEcProperty gRPC request property is invalid or unset: "
               << request->property();
    reply->set_status(
        grpc_api::GetEcPropertyResponse::STATUS_ERROR_REQUIRED_FIELD_MISSING);
    callback.Run(std::move(reply));
    return;
  }

  DCHECK(!base::FilePath(property_file_path).empty());
  base::FilePath sysfs_file_path =
      root_dir_.Append(kEcDriverSysfsPath)
          .Append(kEcDriverSysfsPropertiesPath)
          .Append(base::FilePath(property_file_path));
  // Reply payload must be empty in case of any failure.
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

void WilcoDtcSupportdGrpcService::PerformWebRequest(
    std::unique_ptr<grpc_api::PerformWebRequestParameter> parameter,
    const PerformWebRequestResponseCallback& callback) {
  DCHECK(parameter);
  auto reply = std::make_unique<grpc_api::PerformWebRequestResponse>();

  if (parameter->url().empty()) {
    LOG(ERROR) << "PerformWebRequest URL is empty.";
    reply->set_status(
        grpc_api::PerformWebRequestResponse::STATUS_ERROR_INVALID_URL);
    callback.Run(std::move(reply));
    return;
  }
  if (!base::StartsWith(parameter->url(), kHttpsPrefix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(ERROR) << "PerformWebRequest URL must be an HTTPS URL.";
    reply->set_status(
        grpc_api::PerformWebRequestResponse::STATUS_ERROR_INVALID_URL);
    callback.Run(std::move(reply));
    return;
  }
  if (parameter->headers().size() >
      kMaxNumberOfHeadersInPerformWebRequestParameter) {
    LOG(ERROR) << "PerformWebRequest number of headers is too large.";
    reply->set_status(
        grpc_api::PerformWebRequestResponse::STATUS_ERROR_MAX_SIZE_EXCEEDED);
    callback.Run(std::move(reply));
    return;
  }
  if (CalculateWebRequestParameterSize(parameter) >
      kMaxPerformWebRequestParameterSizeInBytes) {
    LOG(ERROR) << "PerformWebRequest request is too large.";
    reply->set_status(
        grpc_api::PerformWebRequestResponse::STATUS_ERROR_MAX_SIZE_EXCEEDED);
    callback.Run(std::move(reply));
    return;
  }

  DelegateWebRequestHttpMethod delegate_http_method;
  if (!GetDelegateWebRequestHttpMethod(parameter->http_method(),
                                       &delegate_http_method)) {
    reply->set_status(grpc_api::PerformWebRequestResponse ::
                          STATUS_ERROR_REQUIRED_FIELD_MISSING);
    callback.Run(std::move(reply));
    return;
  }
  delegate_->PerformWebRequestToBrowser(
      delegate_http_method, parameter->url(),
      std::vector<std::string>(
          std::make_move_iterator(parameter->mutable_headers()->begin()),
          std::make_move_iterator(parameter->mutable_headers()->end())),
      parameter->request_body(), base::Bind(&ForwardWebGrpcResponse, callback));
}

void WilcoDtcSupportdGrpcService::GetAvailableRoutines(
    std::unique_ptr<grpc_api::GetAvailableRoutinesRequest> request,
    const GetAvailableRoutinesCallback& callback) {
  DCHECK(request);
  delegate_->GetAvailableRoutinesToService(
      base::Bind(&ForwardGetAvailableRoutinesResponse, callback));
}

void WilcoDtcSupportdGrpcService::RunRoutine(
    std::unique_ptr<grpc_api::RunRoutineRequest> request,
    const RunRoutineCallback& callback) {
  DCHECK(request);

  // Make sure the RunRoutineRequest is superficially valid.
  switch (request->routine()) {
    case grpc_api::ROUTINE_BATTERY:
      if (!request->has_battery_params()) {
        LOG(ERROR) << "RunRoutineRequest with routine type BATTERY has no "
                      "battery parameters.";
        ForwardRunRoutineResponse(callback, 0 /* uuid */,
                                  grpc_api::ROUTINE_STATUS_FAILED_TO_START);
        return;
      }
      break;
    case grpc_api::ROUTINE_BATTERY_SYSFS:
      if (!request->has_battery_sysfs_params()) {
        LOG(ERROR) << "RunRoutineRequest with routine type BATTERY_SYSFS has "
                      "no battery_sysfs parameters.";
        ForwardRunRoutineResponse(callback, 0 /* uuid */,
                                  grpc_api::ROUTINE_STATUS_FAILED_TO_START);
        return;
      }
      break;
    case grpc_api::ROUTINE_BAD_BLOCKS:
      // TODO(pmoy@chromium.org): Add check once the BAD_BLOCKS parameters have
      // been defined.
      NOTIMPLEMENTED();
    case grpc_api::ROUTINE_URANDOM:
      if (!request->has_urandom_params()) {
        LOG(ERROR) << "RunRoutineRequest with routine type URANDOM has no "
                      "urandom parameters.";
        ForwardRunRoutineResponse(callback, 0 /* uuid */,
                                  grpc_api::ROUTINE_STATUS_FAILED_TO_START);
        return;
      }
      break;
    case grpc_api::ROUTINE_SMARTCTL_CHECK:
      if (!request->has_smartctl_check_params()) {
        LOG(ERROR) << "RunRoutineRequest with routine type SMARTCTL_CHECK "
                      "has no smartctl_check parameters.";
        ForwardRunRoutineResponse(callback, 0 /* uuid */,
                                  grpc_api::ROUTINE_STATUS_FAILED_TO_START);
        return;
      }
      break;
    default:
      LOG(ERROR) << "RunRoutineRequest routine type invalid or unset.";
      ForwardRunRoutineResponse(callback, 0 /* uuid */,
                                grpc_api::ROUTINE_STATUS_FAILED_TO_START);
      return;
  }

  delegate_->RunRoutineToService(
      *request, base::Bind(&ForwardRunRoutineResponse, callback));
}

void WilcoDtcSupportdGrpcService::GetRoutineUpdate(
    std::unique_ptr<grpc_api::GetRoutineUpdateRequest> request,
    const GetRoutineUpdateCallback& callback) {
  DCHECK(request);

  if (request->command() == grpc_api::GetRoutineUpdateRequest::COMMAND_UNSET) {
    ForwardGetRoutineUpdateResponse(
        callback, request->uuid(), grpc_api::ROUTINE_STATUS_ERROR,
        0 /* progress_percent */, grpc_api::ROUTINE_USER_MESSAGE_UNSET,
        "" /* output */, "No command specified.");
    return;
  }

  delegate_->GetRoutineUpdateRequestToService(
      request->uuid(), request->command(), request->include_output(),
      base::Bind(&ForwardGetRoutineUpdateResponse, callback));
}

void WilcoDtcSupportdGrpcService::GetOsVersion(
    std::unique_ptr<grpc_api::GetOsVersionRequest> request,
    const GetOsVersionCallback& callback) {
  DCHECK(request);

  std::string version;
  if (!base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_VERSION",
                                         &version)) {
    LOG(ERROR) << "Could not read the release version";
  }

  auto reply = std::make_unique<grpc_api::GetOsVersionResponse>();
  reply->set_version(version);
  callback.Run(std::move(reply));
}

void WilcoDtcSupportdGrpcService::GetConfigurationData(
    std::unique_ptr<grpc_api::GetConfigurationDataRequest> request,
    const GetConfigurationDataCallback& callback) {
  DCHECK(request);

  delegate_->GetConfigurationDataFromBrowser(
      base::Bind(&ForwardGetConfigurationDataResponse, callback));
}

void WilcoDtcSupportdGrpcService::GetVpdField(
    std::unique_ptr<grpc_api::GetVpdFieldRequest> request,
    const GetVpdFieldCallback& callback) {
  DCHECK(request);

  auto reply = std::make_unique<grpc_api::GetVpdFieldResponse>();
  switch (request->vpd_field()) {
    case grpc_api::GetVpdFieldRequest::FIELD_SERIAL_NUMBER: {
      std::string vpd_field_value;
      if (!base::ReadFileToString(
              root_dir_.Append(kVpdFieldSerialNumberFilePath),
              &vpd_field_value)) {
        VLOG(2) << "Failed to read VPD field serial number";
        reply->set_status(grpc_api::GetVpdFieldResponse::STATUS_ERROR_INTERNAL);
        break;
      }
      base::TrimString(vpd_field_value, base::kWhitespaceASCII,
                       &vpd_field_value);
      if (vpd_field_value.empty() || !base::IsStringASCII(vpd_field_value)) {
        VLOG(2) << "Serial number is not non-empty ASCII string";
        reply->set_status(grpc_api::GetVpdFieldResponse::STATUS_ERROR_INTERNAL);
        break;
      }

      reply->set_status(grpc_api::GetVpdFieldResponse::STATUS_OK);
      reply->set_vpd_field_value(vpd_field_value);
      break;
    }
    case grpc_api::GetVpdFieldRequest::FIELD_UNSET:
    default:
      VLOG(1) << "The VPD field is unspecified or invalid";
      reply->set_status(
          grpc_api::GetVpdFieldResponse::STATUS_ERROR_VPD_FIELD_UNKNOWN);
      break;
  }
  callback.Run(std::move(reply));
}

void WilcoDtcSupportdGrpcService::GetBluetoothData(
    std::unique_ptr<grpc_api::GetBluetoothDataRequest> request,
    const GetBluetoothDataCallback& callback) {
  NOTIMPLEMENTED();
}

void WilcoDtcSupportdGrpcService::AddFileDump(
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

void WilcoDtcSupportdGrpcService::AddDirectoryDump(
    const base::FilePath& relative_file_path,
    google::protobuf::RepeatedPtrField<grpc_api::FileDump>* file_dumps) {
  DCHECK(!relative_file_path.IsAbsolute());
  std::set<std::string> visited_paths;
  SearchDirectory(root_dir_.Append(relative_file_path), &visited_paths,
                  file_dumps);
}

void WilcoDtcSupportdGrpcService::SearchDirectory(
    const base::FilePath& root_dir,
    std::set<std::string>* visited_paths,
    google::protobuf::RepeatedPtrField<diagnostics::grpc_api::FileDump>*
        file_dumps) {
  visited_paths->insert(base::MakeAbsoluteFilePath(root_dir).value());
  base::FileEnumerator file_enum(
      base::FilePath(root_dir), false,
      base::FileEnumerator::FileType::FILES |
          base::FileEnumerator::FileType::DIRECTORIES |
          base::FileEnumerator::FileType::SHOW_SYM_LINKS);
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    // Only certain symlinks are followed - see the comments for
    // ShouldFollowSymlink for a full description of the behavior.
    if (base::IsLink(path) && !ShouldFollowSymlink(path, root_dir_))
      continue;

    base::FilePath canonical_path = base::MakeAbsoluteFilePath(path);
    if (canonical_path.empty()) {
      VPLOG(2) << "Failed to resolve path.";
      continue;
    }

    // Prevent visiting duplicate paths, which could happen due to following
    // symlinks.
    if (visited_paths->find(canonical_path.value()) != visited_paths->end())
      continue;

    visited_paths->insert(canonical_path.value());

    if (base::DirectoryExists(path)) {
      SearchDirectory(path, visited_paths, file_dumps);
    } else {
      grpc_api::FileDump file_dump;
      if (!MakeFileDump(path, &file_dump)) {
        // When a file is failed to be dumped, it's just omitted from the
        // returned list of entries.
        continue;
      }
      file_dumps->Add()->Swap(&file_dump);
    }
  }
}

}  // namespace diagnostics
