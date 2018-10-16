// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/bind_lambda.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/diagnosticsd/diagnosticsd_grpc_service.h"
#include "diagnostics/diagnosticsd/protobuf_test_utils.h"

#include "diagnosticsd.pb.h"  // NOLINT(build/include)

using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace diagnostics {

namespace {

class MockDiagnosticsdGrpcServiceDelegate
    : public DiagnosticsdGrpcService::Delegate {};

// Tests for the DiagnosticsdGrpcService class.
class DiagnosticsdGrpcServiceTest : public testing::Test {
 protected:
  DiagnosticsdGrpcServiceTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    service_.set_root_dir_for_testing(temp_dir_.GetPath());
  }

  DiagnosticsdGrpcService* service() { return &service_; }

  void CreateDirRecursivelyInTempDir(const base::FilePath& relative_dir_path) {
    if (relative_dir_path == base::FilePath(base::FilePath::kCurrentDirectory))
      return;
    CreateDirRecursivelyInTempDir(relative_dir_path.DirName());
    EXPECT_TRUE(
        base::CreateDirectory(temp_dir_.GetPath().Append(relative_dir_path)));
  }

  void StoreFileInTempDir(const base::FilePath& relative_file_path,
                          const std::string& file_contents) {
    EXPECT_EQ(base::WriteFile(temp_dir_.GetPath().Append(relative_file_path),
                              file_contents.c_str(), file_contents.size()),
              file_contents.size());
  }

  void ExecuteGetProcData(grpc_api::GetProcDataRequest::Type request_type,
                          std::vector<grpc_api::FileDump>* file_dumps) {
    auto request = std::make_unique<grpc_api::GetProcDataRequest>();
    request->set_type(request_type);
    std::unique_ptr<grpc_api::GetProcDataResponse> response;
    service()->GetProcData(
        std::move(request),
        base::Bind(
            [](std::unique_ptr<grpc_api::GetProcDataResponse>* response,
               std::unique_ptr<grpc_api::GetProcDataResponse>
                   received_response) {
              *response = std::move(received_response);
            },
            base::Unretained(&response)));
    // Expect the method to return immediately.
    ASSERT_TRUE(response);
    file_dumps->assign(response->file_dump().begin(),
                       response->file_dump().end());
  }

  grpc_api::FileDump MakeFileDump(
      const base::FilePath& relative_file_path,
      const base::FilePath& canonical_relative_file_path,
      const std::string& file_contents) const {
    grpc_api::FileDump file_dump;
    file_dump.set_path(temp_dir_.GetPath().Append(relative_file_path).value());
    file_dump.set_canonical_path(
        temp_dir_.GetPath().Append(canonical_relative_file_path).value());
    file_dump.set_contents(file_contents);
    return file_dump;
  }

 private:
  base::ScopedTempDir temp_dir_;
  StrictMock<MockDiagnosticsdGrpcServiceDelegate> delegate_;
  DiagnosticsdGrpcService service_{&delegate_};
};

}  // namespace

TEST_F(DiagnosticsdGrpcServiceTest, GetProcDataUnsetType) {
  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetProcData(grpc_api::GetProcDataRequest::TYPE_UNSET, &file_dumps);

  EXPECT_TRUE(file_dumps.empty())
      << "Obtained: "
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

namespace {

// Tests for the GetProcData() method of DiagnosticsdGrpcServiceTest when a
// single file is requested.
//
// This is a parameterized test with the following parameters:
// * |proc_data_request_type| - type of the GetProcData() request to be executed
//   (see GetProcDataRequest::Type);
// * |relative_file_path| - relative path to the file which is expected to be
//    read by the executed GetProcData() request.
class SingleProcFileDiagnosticsdGrpcServiceTest
    : public DiagnosticsdGrpcServiceTest,
      public testing::WithParamInterface<std::tuple<
          grpc_api::GetProcDataRequest::Type /* proc_data_request_type */,
          std::string /* relative_file_path */>> {
 protected:
  static constexpr char kFakeFileContentsChars[] = "\0fake row 1\nfake row 2\n";
  const std::string kFakeFileContents{std::begin(kFakeFileContentsChars),
                                      std::end(kFakeFileContentsChars)};

  // Accessors to individual test parameters from the test parameter tuple
  // returned by gtest's GetParam():

  grpc_api::GetProcDataRequest::Type proc_data_request_type() const {
    return std::get<0>(GetParam());
  }
  base::FilePath relative_file_path() const {
    return base::FilePath(std::get<1>(GetParam()));
  }
};

}  // namespace

// Test that GetProcData() returns a single item with the requested file data
// when the file exists.
TEST_P(SingleProcFileDiagnosticsdGrpcServiceTest, Basic) {
  CreateDirRecursivelyInTempDir(relative_file_path().DirName());
  StoreFileInTempDir(relative_file_path(), kFakeFileContents);

  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetProcData(proc_data_request_type(), &file_dumps);

  const auto kExpectedFileDump = MakeFileDump(
      relative_file_path(), relative_file_path(), kFakeFileContents);
  EXPECT_THAT(file_dumps,
              UnorderedElementsAre(ProtobufEquals(kExpectedFileDump)))
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

// Test that GetProcData() returns empty result when the file doesn't exist.
TEST_P(SingleProcFileDiagnosticsdGrpcServiceTest, NonExisting) {
  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetProcData(proc_data_request_type(), &file_dumps);

  EXPECT_TRUE(file_dumps.empty())
      << "Obtained: "
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

INSTANTIATE_TEST_CASE_P(
    ,
    SingleProcFileDiagnosticsdGrpcServiceTest,
    testing::Values(
        std::tie(grpc_api::GetProcDataRequest::FILE_UPTIME, "proc/uptime"),
        std::tie(grpc_api::GetProcDataRequest::FILE_MEMINFO, "proc/meminfo"),
        std::tie(grpc_api::GetProcDataRequest::FILE_LOADAVG, "proc/loadavg"),
        std::tie(grpc_api::GetProcDataRequest::FILE_STAT, "proc/stat"),
        std::tie(grpc_api::GetProcDataRequest::FILE_NET_NETSTAT,
                 "proc/net/netstat"),
        std::tie(grpc_api::GetProcDataRequest::FILE_NET_DEV, "proc/net/dev")));

}  // namespace diagnostics
