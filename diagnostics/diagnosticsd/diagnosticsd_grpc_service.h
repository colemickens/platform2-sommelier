// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <google/protobuf/repeated_field.h>

#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// The total size of all "string" and "byte" fileds in a single
// PerformWebRequestParameter message must not exceed this size.
extern const int kMaxPerformWebRequestParameterSizeInBytes;

// Max number of headers in PerformWebRequestParameter.
extern const int kMaxNumberOfHeadersInPerformWebRequestParameter;

// Implements the "Diagnosticsd" gRPC interface exposed by the diagnosticsd
// daemon (see the API definition at grpc/diagnosticsd.proto).
class DiagnosticsdGrpcService final {
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
                            std::unique_ptr<std::string> response_body)>;

    virtual ~Delegate() = default;

    // Called when gRPC |PerformWebRequest| was called.
    //
    // Calls diagnosticsd daemon mojo function |PerformWebRequestToBrowser|
    // method and passes all fields of |PerformWebRequestParameter| to perform
    // a web request. The result of the call is returned via |callback|.
    virtual void PerformWebRequestToBrowser(
        WebRequestHttpMethod httpMethod,
        const std::string& url,
        const std::vector<std::string>& headers,
        const std::string& request_body,
        const PerformWebRequestToBrowserCallback& callback) = 0;
  };

  using SendMessageToUiCallback =
      base::Callback<void(std::unique_ptr<grpc_api::SendMessageToUiResponse>)>;
  using GetProcDataCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetProcDataResponse>)>;
  using RunEcCommandCallback =
      base::Callback<void(std::unique_ptr<grpc_api::RunEcCommandResponse>)>;
  using GetEcPropertyCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetEcPropertyResponse>)>;
  using PerformWebRequestResponseCallback = base::Callback<void(
      std::unique_ptr<grpc_api::PerformWebRequestResponse>)>;

  explicit DiagnosticsdGrpcService(Delegate* delegate);
  ~DiagnosticsdGrpcService();

  // Overrides the file system root directory for file operations in tests.
  void set_root_dir_for_testing(const base::FilePath& root_dir) {
    root_dir_ = root_dir;
  }

  // Implementation of the "Diagnosticsd" gRPC interface:
  void SendMessageToUi(
      std::unique_ptr<grpc_api::SendMessageToUiRequest> request,
      const SendMessageToUiCallback& callback);
  void GetProcData(std::unique_ptr<grpc_api::GetProcDataRequest> request,
                   const GetProcDataCallback& callback);
  void RunEcCommand(std::unique_ptr<grpc_api::RunEcCommandRequest> request,
                    const RunEcCommandCallback& callback);
  void GetEcProperty(std::unique_ptr<grpc_api::GetEcPropertyRequest> request,
                     const GetEcPropertyCallback& callback);
  void PerformWebRequest(
      std::unique_ptr<grpc_api::PerformWebRequestParameter> parameter,
      const PerformWebRequestResponseCallback& callback);

 private:
  // Constructs and, if successful, appends the dump of the specified file (with
  // the path given relative to |root_dir_|) to the given protobuf repeated
  // field.
  void AddFileDump(
      const base::FilePath& relative_file_path,
      google::protobuf::RepeatedPtrField<grpc_api::FileDump>* file_dumps);

  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // The file system root directory. Can be overridden in tests.
  base::FilePath root_dir_{"/"};

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdGrpcService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_
