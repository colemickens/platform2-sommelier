// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_GRPC_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_GRPC_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/strings/string_piece.h>
#include <base/macros.h>
#include <google/protobuf/repeated_field.h>

#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// The total size of all "string" and "byte" fileds in a single
// PerformWebRequestParameter message must not exceed this size.
extern const int kMaxPerformWebRequestParameterSizeInBytes;

// Max number of headers in PerformWebRequestParameter.
extern const int kMaxNumberOfHeadersInPerformWebRequestParameter;

// Implements the "WilcoDtcSupportd" gRPC interface exposed by the
// wilco_dtc_supportd daemon (see the API definition at
// grpc/wilco_dtc_supportd.proto)
class WilcoDtcSupportdGrpcService final {
 public:
  class Delegate {
   public:
    // Status of a Web Request performed by |PerformWebRequestToBrowser|.
    enum class WebRequestStatus {
      kOk,
      kNetworkError,
      kHttpError,
      kInternalError,
    };

    // HTTP method to be performed by |PerformWebRequestToBrowser|.
    enum class WebRequestHttpMethod {
      kGet,
      kHead,
      kPost,
      kPut,
    };

    using PerformWebRequestToBrowserCallback =
        base::Callback<void(WebRequestStatus status,
                            int http_status,
                            base::StringPiece response_body)>;
    using GetAvailableRoutinesToServiceCallback = base::Callback<void(
        const std::vector<grpc_api::DiagnosticRoutine>& routines)>;
    using RunRoutineToServiceCallback = base::Callback<void(
        int uuid, grpc_api::DiagnosticRoutineStatus status)>;
    using GetRoutineUpdateRequestToServiceCallback =
        base::Callback<void(int uuid,
                            grpc_api::DiagnosticRoutineStatus status,
                            int progress_percent,
                            grpc_api::DiagnosticRoutineUserMessage user_message,
                            const std::string& output)>;

    virtual ~Delegate() = default;

    // Called when gRPC |PerformWebRequest| was called.
    //
    // Calls wilco_dtc_supportd daemon mojo function
    // |PerformWebRequestToBrowser| method and passes all fields of
    // |PerformWebRequestParameter| to perform a web request.
    // The result of the call is returned via |callback|.
    virtual void PerformWebRequestToBrowser(
        WebRequestHttpMethod httpMethod,
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::string& request_body,
        const PerformWebRequestToBrowserCallback& callback) = 0;
    // Called when gRPC |GetAvailableRoutines| was called.
    //
    // Calls wilco_dtc_supportd daemon routine function |GetAvailableRoutines|
    // method and passes all fields of |GetAvailableRoutinesRequest| to
    // determine which routines are available on the platform. The result
    // of the call is returned via |callback|.
    virtual void GetAvailableRoutinesToService(
        const GetAvailableRoutinesToServiceCallback& callback) = 0;
    // Called when gRPC |RunRoutine| was called.
    //
    // Calls wilco_dtc_supportd daemon routine function |RunRoutine| method and
    // passes all fields of |RunRoutineRequest| to ask the platform to run a
    // diagnostic routine. The result of the call is returned via |callback|.
    virtual void RunRoutineToService(
        const grpc_api::RunRoutineRequest& request,
        const RunRoutineToServiceCallback& callback) = 0;
    // Called when gRPC |GetRoutineUpdate| was called.
    //
    // Calls wilco_dtc_supportd daemon routine function |GetRoutineUpdate|
    // method and passes all fields of |GetRoutineUpdateRequest| to
    // control or get the status of an existing diagnostic routine. The result
    // of the call is returned via |callback|.
    virtual void GetRoutineUpdateRequestToService(
        int uuid,
        grpc_api::GetRoutineUpdateRequest::Command command,
        bool include_output,
        const GetRoutineUpdateRequestToServiceCallback& callback) = 0;
  };

  using SendMessageToUiCallback =
      base::Callback<void(std::unique_ptr<grpc_api::SendMessageToUiResponse>)>;
  using GetProcDataCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetProcDataResponse>)>;
  using GetSysfsDataCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetSysfsDataResponse>)>;
  using RunEcCommandCallback =
      base::Callback<void(std::unique_ptr<grpc_api::RunEcCommandResponse>)>;
  using GetEcPropertyCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetEcPropertyResponse>)>;
  using PerformWebRequestResponseCallback = base::Callback<void(
      std::unique_ptr<grpc_api::PerformWebRequestResponse>)>;
  using GetAvailableRoutinesCallback = base::Callback<void(
      std::unique_ptr<grpc_api::GetAvailableRoutinesResponse>)>;
  using RunRoutineCallback =
      base::Callback<void(std::unique_ptr<grpc_api::RunRoutineResponse>)>;
  using GetRoutineUpdateCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetRoutineUpdateResponse>)>;

  explicit WilcoDtcSupportdGrpcService(Delegate* delegate);
  ~WilcoDtcSupportdGrpcService();

  // Overrides the file system root directory for file operations in tests.
  void set_root_dir_for_testing(const base::FilePath& root_dir) {
    root_dir_ = root_dir;
  }

  // Implementation of the "WilcoDtcSupportd" gRPC interface:
  void SendMessageToUi(
      std::unique_ptr<grpc_api::SendMessageToUiRequest> request,
      const SendMessageToUiCallback& callback);
  void GetProcData(std::unique_ptr<grpc_api::GetProcDataRequest> request,
                   const GetProcDataCallback& callback);
  void GetSysfsData(std::unique_ptr<grpc_api::GetSysfsDataRequest> request,
                    const GetSysfsDataCallback& callback);
  void RunEcCommand(std::unique_ptr<grpc_api::RunEcCommandRequest> request,
                    const RunEcCommandCallback& callback);
  void GetEcProperty(std::unique_ptr<grpc_api::GetEcPropertyRequest> request,
                     const GetEcPropertyCallback& callback);
  void PerformWebRequest(
      std::unique_ptr<grpc_api::PerformWebRequestParameter> parameter,
      const PerformWebRequestResponseCallback& callback);
  void GetAvailableRoutines(
      std::unique_ptr<grpc_api::GetAvailableRoutinesRequest> request,
      const GetAvailableRoutinesCallback& callback);
  void RunRoutine(std::unique_ptr<grpc_api::RunRoutineRequest> request,
                  const RunRoutineCallback& callback);
  void GetRoutineUpdate(
      std::unique_ptr<grpc_api::GetRoutineUpdateRequest> request,
      const GetRoutineUpdateCallback& callback);

 private:
  // Constructs and, if successful, appends the dump of the specified file (with
  // the path given relative to |root_dir_|) to the given protobuf repeated
  // field.
  void AddFileDump(
      const base::FilePath& relative_file_path,
      google::protobuf::RepeatedPtrField<grpc_api::FileDump>* file_dumps);
  // Constructs and, if successful, appends the dump of every file in the
  // specified directory (with the path given relative to |root_dir_|) to the
  // given protobuf repeated field. This will follow allowable symlinks - see
  // ShouldFollowSymlink() for details.
  void AddDirectoryDump(
      const base::FilePath& relative_file_path,
      google::protobuf::RepeatedPtrField<grpc_api::FileDump>* file_dumps);
  void SearchDirectory(
      const base::FilePath& root_dir,
      std::set<std::string>* visited_paths,
      google::protobuf::RepeatedPtrField<diagnostics::grpc_api::FileDump>*
          file_dumps);

  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // The file system root directory. Can be overridden in tests.
  base::FilePath root_dir_{"/"};

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdGrpcService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_GRPC_SERVICE_H_
