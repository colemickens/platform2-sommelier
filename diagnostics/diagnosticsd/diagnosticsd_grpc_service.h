// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_

#include <memory>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <google/protobuf/repeated_field.h>

#include "common.pb.h"        // NOLINT(build/include)
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Implements the "Diagnosticsd" gRPC interface exposed by the diagnosticsd
// daemon (see the API definition at grpc/diagnosticsd.proto).
class DiagnosticsdGrpcService final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
  };

  using SendMessageToUiCallback =
      base::Callback<void(std::unique_ptr<grpc_api::EmptyMessage>)>;
  using GetProcDataCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetProcDataResponse>)>;

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
