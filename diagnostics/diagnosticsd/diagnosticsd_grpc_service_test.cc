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
#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/bind_lambda.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/diagnosticsd/diagnosticsd_grpc_service.h"
#include "diagnostics/diagnosticsd/file_test_utils.h"
#include "diagnostics/diagnosticsd/protobuf_test_utils.h"

#include "diagnosticsd.pb.h"  // NOLINT(build/include)

using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace diagnostics {

TEST(DiagnosticsdGrpcServiceConstantsTest, PropertiesPath) {
  EXPECT_EQ(
      base::FilePath(kEcDriverSysfsPath).Append(kEcDriverSysfsPropertiesPath),
      base::FilePath("sys/bus/platform/devices/GOOG000C:00/properties/"));
}

TEST(DiagnosticsdGrpcServiceConstantsTest, RawFilePath) {
  EXPECT_EQ(base::FilePath(kEcDriverSysfsPath).Append(kEcRunCommandFilePath),
            base::FilePath("sys/bus/platform/devices/GOOG000C:00/raw"));
}

namespace {

constexpr char kFakeFileContentsChars[] = "\0fake row 1\nfake row 2\n\0\377";

std::string FakeFileContents() {
  return std::string(std::begin(kFakeFileContentsChars),
                     std::end(kFakeFileContentsChars));
}

template <class T>
base::Callback<void(std::unique_ptr<T>)> GrpcCallbackResponseSaver(
    std::unique_ptr<T>* response) {
  return base::Bind(
      [](std::unique_ptr<T>* response, std::unique_ptr<T> received_response) {
        *response = std::move(received_response);
        ASSERT_TRUE(*response);
      },
      base::Unretained(response));
}

std::unique_ptr<grpc_api::RunEcCommandResponse> MakeRunEcCommandResponse(
    grpc_api::RunEcCommandResponse::Status status, const std::string& payload) {
  auto response = std::make_unique<grpc_api::RunEcCommandResponse>();
  response->set_status(status);
  response->set_payload(payload);
  return response;
}

std::unique_ptr<grpc_api::GetEcPropertyResponse> MakeEcPropertyResponse(
    grpc_api::GetEcPropertyResponse::Status status,
    const std::string& payload) {
  auto response = std::make_unique<grpc_api::GetEcPropertyResponse>();
  response->set_status(status);
  response->set_payload(payload);
  return response;
}

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

  void ExecuteGetProcData(grpc_api::GetProcDataRequest::Type request_type,
                          std::vector<grpc_api::FileDump>* file_dumps) {
    auto request = std::make_unique<grpc_api::GetProcDataRequest>();
    request->set_type(request_type);
    std::unique_ptr<grpc_api::GetProcDataResponse> response;
    service()->GetProcData(std::move(request),
                           GrpcCallbackResponseSaver(&response));

    // Expect the method to return immediately.
    ASSERT_TRUE(response);
    file_dumps->assign(response->file_dump().begin(),
                       response->file_dump().end());
  }

  void ExecuteRunEcCommand(
      const std::string request_payload,
      std::unique_ptr<grpc_api::RunEcCommandResponse>* response) {
    auto request = std::make_unique<grpc_api::RunEcCommandRequest>();
    request->set_payload(request_payload);

    service()->RunEcCommand(std::move(request),
                            GrpcCallbackResponseSaver(response));
    ASSERT_TRUE(response);
  }

  void ExecuteGetEcProperty(
      grpc_api::GetEcPropertyRequest::Property request_property,
      std::unique_ptr<grpc_api::GetEcPropertyResponse>* response) {
    auto request = std::make_unique<grpc_api::GetEcPropertyRequest>();
    request->set_property(request_property);

    service()->GetEcProperty(std::move(request),
                             GrpcCallbackResponseSaver(response));
    ASSERT_TRUE(response);
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

  base::FilePath temp_dir_path() const { return temp_dir_.GetPath(); }

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

// Test that RunEcCommand() response contains expected |status| and |payload|
// field values.
TEST_F(DiagnosticsdGrpcServiceTest, RunEcCommandErrorAccessingDriver) {
  std::unique_ptr<grpc_api::RunEcCommandResponse> response;
  ExecuteRunEcCommand(FakeFileContents(), &response);
  ASSERT_TRUE(response);
  auto expected_response = MakeRunEcCommandResponse(
      grpc_api::RunEcCommandResponse::STATUS_ERROR_ACCESSING_DRIVER, "");
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that GetEcProperty() returns invalid property error status when
// property is unset or invalid.
TEST_F(DiagnosticsdGrpcServiceTest, GetEcPropertyInputPropertyIsUnset) {
  std::unique_ptr<grpc_api::GetEcPropertyResponse> response;
  ExecuteGetEcProperty(grpc_api::GetEcPropertyRequest::PROPERTY_UNSET,
                       &response);
  auto expected_response = MakeEcPropertyResponse(
      grpc_api::GetEcPropertyResponse::STATUS_ERROR_REQUIRED_FIELD_MISSING,
      std::string());
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
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
  // Accessors to individual test parameters from the test parameter tuple
  // returned by gtest's GetParam():

  grpc_api::GetProcDataRequest::Type proc_data_request_type() const {
    return std::get<0>(GetParam());
  }
  base::FilePath relative_file_path() const {
    return base::FilePath(std::get<1>(GetParam()));
  }

  base::FilePath absolute_file_path() const {
    return temp_dir_path().Append(relative_file_path());
  }
};

}  // namespace

// Test that GetProcData() returns a single item with the requested file data
// when the file exists.
TEST_P(SingleProcFileDiagnosticsdGrpcServiceTest, Basic) {
  EXPECT_TRUE(
      WriteFileAndCreateParentDirs(absolute_file_path(), FakeFileContents()));

  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetProcData(proc_data_request_type(), &file_dumps);

  const auto kExpectedFileDump = MakeFileDump(
      relative_file_path(), relative_file_path(), FakeFileContents());
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

namespace {

// Tests for the RunEcCommand() method of DiagnosticsdGrpcServiceTest.
//
// This is a parameterized test with the following parameters:
// * |request_payload| - payload of the RunEcCommand() request;
// * |expected_response_status| - expected RunEcCommand() response status;
// * |expected_response_payload| - expected RunEcCommand() response payload.
class RunEcCommandDiagnosticsdGrpcServiceTest
    : public DiagnosticsdGrpcServiceTest,
      public testing::WithParamInterface<std::tuple<
          std::string /* request_payload */,
          grpc_api::RunEcCommandResponse::Status /* expected_response_status */,
          std::string /* expected_response_payload */>> {
 protected:
  std::string request_payload() const { return std::get<0>(GetParam()); }

  grpc_api::RunEcCommandResponse::Status expected_response_status() const {
    return std::get<1>(GetParam());
  }

  std::string expected_response_payload() const {
    return std::get<2>(GetParam());
  }

  base::FilePath sysfs_raw_file() const {
    return temp_dir_path()
        .Append(kEcDriverSysfsPath)
        .Append(kEcRunCommandFilePath);
  }
};

}  // namespace

// Test that RunEcCommand() response contains expected |status| and |payload|
// field values.
TEST_P(RunEcCommandDiagnosticsdGrpcServiceTest, Base) {
  EXPECT_TRUE(WriteFileAndCreateParentDirs(sysfs_raw_file(), ""));
  std::unique_ptr<grpc_api::RunEcCommandResponse> response;
  ExecuteRunEcCommand(request_payload(), &response);
  ASSERT_TRUE(response);
  auto expected_response = MakeRunEcCommandResponse(
      expected_response_status(), expected_response_payload());
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

INSTANTIATE_TEST_CASE_P(
    ,
    RunEcCommandDiagnosticsdGrpcServiceTest,
    testing::Values(
        std::make_tuple(FakeFileContents(),
                        grpc_api::RunEcCommandResponse::STATUS_OK,
                        FakeFileContents()),
        std::make_tuple(std::string("A", kRunEcCommandPayloadMaxSize),
                        grpc_api::RunEcCommandResponse::STATUS_OK,
                        std::string("A", kRunEcCommandPayloadMaxSize)),
        std::make_tuple(
            "",
            grpc_api::RunEcCommandResponse::STATUS_ERROR_INPUT_PAYLOAD_EMPTY,
            ""),
        std::make_tuple(std::string("A", kRunEcCommandPayloadMaxSize + 1),
                        grpc_api::RunEcCommandResponse::
                            STATUS_ERROR_INPUT_PAYLOAD_MAX_SIZE_EXCEEDED,
                        "")));

namespace {

// Tests for the GetEcProperty() method of DiagnosticsdGrpcServiceTest.
//
// This is a parameterized test with the following parameters:
// * |ec_property| - property of the GetEcProperty() request to be executed
//   (see GetEcPropertyRequest::Property);
// * |sysfs_file_name| - sysfs file in |kEcDriverSysfsPropertiesPath|
//   properties folder which is expected to be read by the executed
//   GetEcProperty() request.
class GetEcPropertyDiagnosticsdGrpcServiceTest
    : public DiagnosticsdGrpcServiceTest,
      public testing::WithParamInterface<
          std::tuple<grpc_api::GetEcPropertyRequest::Property /* ec_property */,
                     std::string /* sysfs_file_name */>> {
 protected:
  grpc_api::GetEcPropertyRequest::Property ec_property() {
    return std::get<0>(GetParam());
  }

  std::string sysfs_file_name() const { return std::get<1>(GetParam()); }

  base::FilePath sysfs_file_path() const {
    return temp_dir_path()
        .Append(kEcDriverSysfsPath)
        .Append(kEcDriverSysfsPropertiesPath)
        .Append(sysfs_file_name());
  }
};

}  // namespace

// Test that GetEcProperty() returns EC property value when appropriate
// sysfs file exists.
TEST_P(GetEcPropertyDiagnosticsdGrpcServiceTest, SysfsFileExists) {
  EXPECT_TRUE(
      WriteFileAndCreateParentDirs(sysfs_file_path(), FakeFileContents()));
  std::unique_ptr<grpc_api::GetEcPropertyResponse> response;
  ExecuteGetEcProperty(ec_property(), &response);
  ASSERT_TRUE(response);
  auto expected_response = MakeEcPropertyResponse(
      grpc_api::GetEcPropertyResponse::STATUS_OK, FakeFileContents());
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that GetEcProperty() returns accessing driver error status when
// appropriate sysfs file does not exist.
TEST_P(GetEcPropertyDiagnosticsdGrpcServiceTest, SysfsFileDoesNotExist) {
  std::unique_ptr<grpc_api::GetEcPropertyResponse> response;
  ExecuteGetEcProperty(ec_property(), &response);
  ASSERT_TRUE(response);
  auto expected_response = MakeEcPropertyResponse(
      grpc_api::GetEcPropertyResponse::STATUS_ERROR_ACCESSING_DRIVER,
      std::string());
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

INSTANTIATE_TEST_CASE_P(
    ,
    GetEcPropertyDiagnosticsdGrpcServiceTest,
    testing::Values(
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_GLOBAL_MIC_MUTE_LED,
                 kEcPropertyGlobalMicMuteLed)));

}  // namespace diagnostics
