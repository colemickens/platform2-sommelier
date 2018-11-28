// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_GRPC_SERVICE_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <google/protobuf/repeated_field.h>

#include "common.pb.h"        // NOLINT(build/include)
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Folder path exposed by sysfs EC driver.
extern const char kEcDriverSysfsPath[];

// Folder path to EC properties exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
extern const char kEcDriverSysfsPropertiesPath[];

// EC property |global_mic_mute_led|.
extern const char kEcPropertyGlobalMicMuteLed[];

// Max RunEcCommand request payload size.
//
// TODO(lamzin): replace by real payload max size when EC driver will be ready.
extern const int64_t kRunEcCommandPayloadMaxSize;

// File for running EC command exposed by sysfs EC driver. Relative path to
// |kEcDriverSysfsPath|.
//
// TODO(lamzin): replace by real file path when EC driver will be ready.
extern const char kEcRunCommandFilePath[];

// Implements the "Diagnosticsd" gRPC interface exposed by the diagnosticsd
// daemon (see the API definition at grpc/diagnosticsd.proto).
class DiagnosticsdGrpcService final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
  };

  using SendMessageToUiCallback =
      base::Callback<void(std::unique_ptr<grpc_api::SendMessageToUiResponse>)>;
  using GetProcDataCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetProcDataResponse>)>;
  using RunEcCommandCallback =
      base::Callback<void(std::unique_ptr<grpc_api::RunEcCommandResponse>)>;
  using GetEcPropertyCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetEcPropertyResponse>)>;

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
