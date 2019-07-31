// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_piece.h>
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include <gmock/gmock.h>
#include <google/protobuf/repeated_field.h>
#include <gtest/gtest.h>

#include "diagnostics/common/file_test_utils.h"
#include "diagnostics/wilco_dtc_supportd/ec_constants.h"
#include "diagnostics/wilco_dtc_supportd/protobuf_test_utils.h"
#include "diagnostics/wilco_dtc_supportd/vpd_constants.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_grpc_service.h"

#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

using testing::_;
using testing::AnyOf;
using testing::Eq;
using testing::Invoke;
using testing::StrEq;
using testing::StrictMock;
using testing::UnorderedElementsAre;
using testing::WithArgs;

namespace diagnostics {

namespace {

using DelegateWebRequestHttpMethod =
    WilcoDtcSupportdGrpcService::Delegate::WebRequestHttpMethod;
using DelegateWebRequestStatus =
    WilcoDtcSupportdGrpcService::Delegate::WebRequestStatus;

constexpr char kFakeFileContentsChars[] = "\0fake row 1\nfake row 2\n\0\377";
constexpr char kFakeSecondFileContentsChars[] =
    "\0fake col 1\nfake col 2\n\0\377";

constexpr int kHttpStatusOk = 200;
constexpr char kBadNonHttpsUrl[] = "Http://www.google.com";
constexpr char kCorrectUrl[] = "hTTps://www.google.com";
constexpr char kFakeWebResponseBody[] = "\0Fake WEB\n response body\n\0";
const DelegateWebRequestHttpMethod kDelegateWebRequestHttpMethodGet =
    DelegateWebRequestHttpMethod::kGet;
const DelegateWebRequestHttpMethod kDelegateWebRequestHttpMethodHead =
    DelegateWebRequestHttpMethod::kHead;
const DelegateWebRequestHttpMethod kDelegateWebRequestHttpMethodPost =
    DelegateWebRequestHttpMethod::kPost;
const DelegateWebRequestHttpMethod kDelegateWebRequestHttpMethodPut =
    DelegateWebRequestHttpMethod::kPut;

constexpr grpc_api::DiagnosticRoutine kFakeAvailableRoutines[] = {
    grpc_api::ROUTINE_BATTERY, grpc_api::ROUTINE_BATTERY_SYSFS,
    grpc_api::ROUTINE_SMARTCTL_CHECK, grpc_api::ROUTINE_URANDOM};
constexpr int kFakeUuid = 13;
constexpr grpc_api::DiagnosticRoutineStatus kFakeStatus =
    grpc_api::ROUTINE_STATUS_RUNNING;
constexpr int kFakeProgressPercent = 37;
constexpr grpc_api::DiagnosticRoutineUserMessage kFakeUserMessage =
    grpc_api::ROUTINE_USER_MESSAGE_UNSET;
constexpr char kFakeOutput[] = "Some output.";
constexpr char kFakeStatusMessage[] = "Status message.";
constexpr char kFakeSerialNumber[] = "fakeserialnumber";

std::string FakeFileContents() {
  return std::string(std::begin(kFakeFileContentsChars),
                     std::end(kFakeFileContentsChars));
}

std::string FakeSecondFileContents() {
  return std::string(std::begin(kFakeSecondFileContentsChars),
                     std::end(kFakeSecondFileContentsChars));
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

std::unique_ptr<grpc_api::GetEcTelemetryResponse> MakeGetEcTelemetryResponse(
    grpc_api::GetEcTelemetryResponse::Status status,
    const std::string& payload) {
  auto response = std::make_unique<grpc_api::GetEcTelemetryResponse>();
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

std::unique_ptr<grpc_api::PerformWebRequestResponse>
MakePerformWebRequestResponse(
    grpc_api::PerformWebRequestResponse::Status status,
    const int* http_status,
    const char* response_body) {
  auto response = std::make_unique<grpc_api::PerformWebRequestResponse>();
  response->set_status(status);
  if (http_status)
    response->set_http_status(*http_status);
  if (response_body)
    response->set_response_body(response_body);
  return response;
}

std::unique_ptr<grpc_api::GetAvailableRoutinesResponse>
MakeGetAvailableRoutinesResponse() {
  auto response = std::make_unique<grpc_api::GetAvailableRoutinesResponse>();
  for (auto routine : kFakeAvailableRoutines)
    response->add_routines(routine);
  return response;
}

std::unique_ptr<grpc_api::RunRoutineResponse> MakeRunRoutineResponse() {
  auto response = std::make_unique<grpc_api::RunRoutineResponse>();
  response->set_uuid(kFakeUuid);
  response->set_status(kFakeStatus);
  return response;
}

std::unique_ptr<grpc_api::GetRoutineUpdateResponse>
MakeGetRoutineUpdateResponse(int uuid, bool include_output) {
  auto response = std::make_unique<grpc_api::GetRoutineUpdateResponse>();
  response->set_uuid(uuid);
  response->set_status(kFakeStatus);
  response->set_progress_percent(kFakeProgressPercent);
  response->set_user_message(kFakeUserMessage);
  response->set_output(include_output ? kFakeOutput : "");
  response->set_status_message(kFakeStatusMessage);
  return response;
}

std::unique_ptr<grpc_api::RunRoutineRequest> MakeRunBatteryRoutineRequest() {
  constexpr int kLowmAh = 10;
  constexpr int kHighmAh = 100;
  auto request = std::make_unique<grpc_api::RunRoutineRequest>();
  request->set_routine(grpc_api::ROUTINE_BATTERY);
  request->mutable_battery_params()->set_low_mah(kLowmAh);
  request->mutable_battery_params()->set_high_mah(kHighmAh);
  return request;
}

std::unique_ptr<grpc_api::RunRoutineRequest>
MakeRunBatterySysfsRoutineRequest() {
  constexpr int kMaximumCycleCount = 5;
  constexpr int kPercentBatteryWearAllowed = 10;
  auto request = std::make_unique<grpc_api::RunRoutineRequest>();
  request->set_routine(grpc_api::ROUTINE_BATTERY_SYSFS);
  request->mutable_battery_sysfs_params()->set_maximum_cycle_count(
      kMaximumCycleCount);
  request->mutable_battery_sysfs_params()->set_percent_battery_wear_allowed(
      kPercentBatteryWearAllowed);
  return request;
}

std::unique_ptr<grpc_api::RunRoutineRequest> MakeRunUrandomRoutineRequest() {
  constexpr int kLengthSeconds = 10;
  auto request = std::make_unique<grpc_api::RunRoutineRequest>();
  request->set_routine(grpc_api::ROUTINE_URANDOM);
  request->mutable_urandom_params()->set_length_seconds(kLengthSeconds);
  return request;
}

class MockWilcoDtcSupportdGrpcServiceDelegate
    : public WilcoDtcSupportdGrpcService::Delegate {
 public:
  // WilcoDtcSupportdGrpcService::Delegate overrides:
  MOCK_METHOD5(PerformWebRequestToBrowser,
               void(WebRequestHttpMethod http_method,
                    const std::string& url,
                    const std::vector<std::string>& headers,
                    const std::string& request_body,
                    const PerformWebRequestToBrowserCallback& callback));
  MOCK_METHOD1(GetAvailableRoutinesToService,
               void(const GetAvailableRoutinesToServiceCallback& callback));
  MOCK_METHOD2(RunRoutineToService,
               void(const grpc_api::RunRoutineRequest& request,
                    const RunRoutineToServiceCallback& callback));
  MOCK_METHOD4(GetRoutineUpdateRequestToService,
               void(const int uuid,
                    const grpc_api::GetRoutineUpdateRequest::Command command,
                    const bool include_output,
                    const GetRoutineUpdateRequestToServiceCallback& callback));
  MOCK_METHOD1(GetConfigurationDataFromBrowser,
               void(const GetConfigurationDataFromBrowserCallback& callback));
};

// Tests for the WilcoDtcSupportdGrpcService class.
class WilcoDtcSupportdGrpcServiceTest : public testing::Test {
 protected:
  WilcoDtcSupportdGrpcServiceTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    service_.set_root_dir_for_testing(temp_dir_.GetPath());
  }

  WilcoDtcSupportdGrpcService* service() { return &service_; }

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

  void ExecuteGetSysfsData(grpc_api::GetSysfsDataRequest::Type request_type,
                           std::vector<grpc_api::FileDump>* file_dumps) {
    auto request = std::make_unique<grpc_api::GetSysfsDataRequest>();
    request->set_type(request_type);
    std::unique_ptr<grpc_api::GetSysfsDataResponse> response;
    service()->GetSysfsData(std::move(request),
                            GrpcCallbackResponseSaver(&response));

    // Expect the method to return immediately.
    ASSERT_TRUE(response);
    file_dumps->assign(response->file_dump().begin(),
                       response->file_dump().end());
  }

  void ExecuteGetEcTelemetry(
      const std::string request_payload,
      std::unique_ptr<grpc_api::GetEcTelemetryResponse>* response) {
    auto request = std::make_unique<grpc_api::GetEcTelemetryRequest>();
    request->set_payload(request_payload);

    service()->GetEcTelemetry(std::move(request),
                              GrpcCallbackResponseSaver(response));
    ASSERT_TRUE(*response);
  }

  void ExecuteGetEcProperty(
      grpc_api::GetEcPropertyRequest::Property request_property,
      std::unique_ptr<grpc_api::GetEcPropertyResponse>* response) {
    auto request = std::make_unique<grpc_api::GetEcPropertyRequest>();
    request->set_property(request_property);

    service()->GetEcProperty(std::move(request),
                             GrpcCallbackResponseSaver(response));
    ASSERT_TRUE(*response);
  }

  void ExecutePerformWebRequest(
      grpc_api::PerformWebRequestParameter::HttpMethod http_method,
      const std::string& url,
      const std::vector<std::string>& string_headers,
      const std::string& request_body,
      const DelegateWebRequestHttpMethod* delegate_http_method,
      std::unique_ptr<grpc_api::PerformWebRequestResponse>* response) {
    auto request = std::make_unique<grpc_api::PerformWebRequestParameter>();
    request->set_http_method(http_method);
    request->set_url(url);

    google::protobuf::RepeatedPtrField<std::string> headers(
        string_headers.begin(), string_headers.end());
    request->mutable_headers()->Swap(&headers);

    request->set_request_body(request_body);

    base::Callback<void(DelegateWebRequestStatus, int)> callback;
    if (delegate_http_method) {
      EXPECT_CALL(delegate_,
                  PerformWebRequestToBrowser(Eq(*delegate_http_method), url,
                                             string_headers, request_body, _))
          .WillOnce(WithArgs<4>(Invoke(
              [](const base::Callback<void(DelegateWebRequestStatus, int,
                                           base::StringPiece)>& callback) {
                callback.Run(DelegateWebRequestStatus::kOk, kHttpStatusOk,
                             kFakeWebResponseBody);
              })));
    }
    service()->PerformWebRequest(std::move(request),
                                 GrpcCallbackResponseSaver(response));
  }

  void ExecuteGetAvailableRoutines(
      std::unique_ptr<grpc_api::GetAvailableRoutinesResponse>* response) {
    auto request = std::make_unique<grpc_api::GetAvailableRoutinesRequest>();
    EXPECT_CALL(delegate_, GetAvailableRoutinesToService(_))
        .WillOnce(
            Invoke([](const base::Callback<void(
                          const std::vector<grpc_api::DiagnosticRoutine>&)>&
                          callback) {
              callback.Run(std::vector<grpc_api::DiagnosticRoutine>(
                  std::begin(kFakeAvailableRoutines),
                  std::end(kFakeAvailableRoutines)));
            }));
    service()->GetAvailableRoutines(std::move(request),
                                    GrpcCallbackResponseSaver(response));
  }

  void ExecuteRunRoutine(
      std::unique_ptr<grpc_api::RunRoutineRequest> request,
      std::unique_ptr<grpc_api::RunRoutineResponse>* response,
      bool is_valid_request) {
    if (is_valid_request) {
      EXPECT_CALL(delegate_, RunRoutineToService(_, _))
          .WillOnce(WithArgs<1>(Invoke(
              [](const base::Callback<void(
                     int, grpc_api::DiagnosticRoutineStatus)>& callback) {
                callback.Run(kFakeUuid, kFakeStatus);
              })));
    }
    service()->RunRoutine(std::move(request),
                          GrpcCallbackResponseSaver(response));
  }

  void ExecuteGetRoutineUpdate(
      int uuid,
      grpc_api::GetRoutineUpdateRequest::Command command,
      bool include_output,
      std::unique_ptr<grpc_api::GetRoutineUpdateResponse>* response) {
    if (command != grpc_api::GetRoutineUpdateRequest::COMMAND_UNSET) {
      EXPECT_CALL(delegate_, GetRoutineUpdateRequestToService(
                                 uuid, command, include_output, _))
          .WillOnce(WithArgs<3>(Invoke(
              [=](const base::Callback<void(
                      int, grpc_api::DiagnosticRoutineStatus, int,
                      grpc_api::DiagnosticRoutineUserMessage,
                      const std::string&, const std::string&)>& callback) {
                callback.Run(
                    uuid, kFakeStatus, kFakeProgressPercent, kFakeUserMessage,
                    include_output ? kFakeOutput : "", kFakeStatusMessage);
              })));
    }
    auto request = std::make_unique<grpc_api::GetRoutineUpdateRequest>();
    request->set_uuid(uuid);
    request->set_command(command);
    request->set_include_output(include_output);
    service()->GetRoutineUpdate(std::move(request),
                                GrpcCallbackResponseSaver(response));
  }

  void ExecuteGetOsVersion(std::string* version) {
    auto request = std::make_unique<grpc_api::GetOsVersionRequest>();
    std::unique_ptr<grpc_api::GetOsVersionResponse> response;
    service()->GetOsVersion(std::move(request),
                            GrpcCallbackResponseSaver(&response));

    // Expect the method to return immediately.
    ASSERT_TRUE(response);
    *version = response->version();
  }

  void ExecuteGetConfigurationData(
      const std::string& json_configuration_data,
      std::unique_ptr<grpc_api::GetConfigurationDataResponse>* response) {
    auto request = std::make_unique<grpc_api::GetConfigurationDataRequest>();
    EXPECT_CALL(delegate_, GetConfigurationDataFromBrowser(_))
        .WillOnce(WithArgs<0>(Invoke(
            [json_configuration_data](
                const base::Callback<void(const std::string&)>& callback) {
              callback.Run(json_configuration_data);
            })));
    service()->GetConfigurationData(std::move(request),
                                    GrpcCallbackResponseSaver(response));
  }

  void ExecuteGetVpdField(grpc_api::GetVpdFieldRequest::VpdField vpd_field,
                          grpc_api::GetVpdFieldResponse::Status* status,
                          std::string* vpd_field_value) {
    auto request = std::make_unique<grpc_api::GetVpdFieldRequest>();
    request->set_vpd_field(vpd_field);
    std::unique_ptr<grpc_api::GetVpdFieldResponse> response;
    service()->GetVpdField(std::move(request),
                           GrpcCallbackResponseSaver(&response));

    // Expect the method to return immediately.
    ASSERT_TRUE(response);
    *status = response->status();
    *vpd_field_value = response->vpd_field_value();
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
  StrictMock<MockWilcoDtcSupportdGrpcServiceDelegate> delegate_;
  WilcoDtcSupportdGrpcService service_{&delegate_};
};

TEST_F(WilcoDtcSupportdGrpcServiceTest, GetProcDataUnsetType) {
  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetProcData(grpc_api::GetProcDataRequest::TYPE_UNSET, &file_dumps);

  EXPECT_TRUE(file_dumps.empty())
      << "Obtained: "
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

TEST_F(WilcoDtcSupportdGrpcServiceTest, GetSysfsDataUnsetType) {
  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetSysfsData(grpc_api::GetSysfsDataRequest::TYPE_UNSET, &file_dumps);

  EXPECT_TRUE(file_dumps.empty())
      << "Obtained: "
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

TEST_F(WilcoDtcSupportdGrpcServiceTest, RunRoutineUnsetType) {
  auto request = std::make_unique<grpc_api::RunRoutineRequest>();
  request->set_routine(grpc_api::ROUTINE_UNSET);
  auto response = std::make_unique<grpc_api::RunRoutineResponse>();
  ExecuteRunRoutine(std::move(request), &response,
                    false /* is_valid_request */);
  EXPECT_EQ(response->uuid(), 0);
  EXPECT_EQ(response->status(), grpc_api::ROUTINE_STATUS_FAILED_TO_START);
}

TEST_F(WilcoDtcSupportdGrpcServiceTest, GetRoutineUpdateUnsetType) {
  std::unique_ptr<grpc_api::GetRoutineUpdateResponse> response;
  constexpr bool kIncludeOutput = false;
  ExecuteGetRoutineUpdate(kFakeUuid,
                          grpc_api::GetRoutineUpdateRequest::COMMAND_UNSET,
                          kIncludeOutput, &response);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->uuid(), kFakeUuid);
  EXPECT_EQ(response->status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that GetEcTelemetry() response contains expected |status| and |payload|
// field values.
TEST_F(WilcoDtcSupportdGrpcServiceTest, GetEcTelemetryErrorAccessingDriver) {
  std::unique_ptr<grpc_api::GetEcTelemetryResponse> response;
  ExecuteGetEcTelemetry(FakeFileContents(), &response);
  ASSERT_TRUE(response);
  auto expected_response = MakeGetEcTelemetryResponse(
      grpc_api::GetEcTelemetryResponse::STATUS_ERROR_ACCESSING_DRIVER, "");
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that GetEcProperty() returns invalid property error status when
// property is unset or invalid.
TEST_F(WilcoDtcSupportdGrpcServiceTest, GetEcPropertyInputPropertyIsUnset) {
  std::unique_ptr<grpc_api::GetEcPropertyResponse> response;
  ExecuteGetEcProperty(grpc_api::GetEcPropertyRequest::PROPERTY_UNSET,
                       &response);
  auto expected_response = MakeEcPropertyResponse(
      grpc_api::GetEcPropertyResponse::STATUS_ERROR_REQUIRED_FIELD_MISSING,
      std::string());
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that GetAvailableRoutines returns the expected list of diagnostic
// routines.
TEST_F(WilcoDtcSupportdGrpcServiceTest, GetAvailableRoutines) {
  std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> response;
  ExecuteGetAvailableRoutines(&response);
  auto expected_response = MakeGetAvailableRoutinesResponse();
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that we can request that the battery routine be run.
TEST_F(WilcoDtcSupportdGrpcServiceTest, RunBatteryRoutine) {
  std::unique_ptr<grpc_api::RunRoutineResponse> response;
  ExecuteRunRoutine(MakeRunBatteryRoutineRequest(), &response,
                    true /* is_valid_request */);
  auto expected_response = MakeRunRoutineResponse();
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that a battery routine with no parameters will fail.
TEST_F(WilcoDtcSupportdGrpcServiceTest, RunBatteryRoutineNoParameters) {
  std::unique_ptr<grpc_api::RunRoutineResponse> response;
  auto request = std::make_unique<grpc_api::RunRoutineRequest>();
  request->set_routine(grpc_api::ROUTINE_BATTERY);
  ExecuteRunRoutine(std::move(request), &response,
                    false /* is_valid_request */);
  EXPECT_EQ(response->uuid(), 0);
  EXPECT_EQ(response->status(), grpc_api::ROUTINE_STATUS_FAILED_TO_START);
}

// Test that we can request that the battery_sysfs routine be run.
TEST_F(WilcoDtcSupportdGrpcServiceTest, RunBatterySysfsRoutine) {
  std::unique_ptr<grpc_api::RunRoutineResponse> response;
  ExecuteRunRoutine(MakeRunBatterySysfsRoutineRequest(), &response,
                    true /* is_valid_request */);
  auto expected_response = MakeRunRoutineResponse();
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that a battery_sysfs routine with no parameters will fail.
TEST_F(WilcoDtcSupportdGrpcServiceTest, RunBatterySysfsRoutineNoParameters) {
  std::unique_ptr<grpc_api::RunRoutineResponse> response;
  auto request = std::make_unique<grpc_api::RunRoutineRequest>();
  request->set_routine(grpc_api::ROUTINE_BATTERY_SYSFS);
  ExecuteRunRoutine(std::move(request), &response,
                    false /* is_valid_request */);
  EXPECT_EQ(response->uuid(), 0);
  EXPECT_EQ(response->status(), grpc_api::ROUTINE_STATUS_FAILED_TO_START);
}

// Test that we can request that the urandom routine be run.
TEST_F(WilcoDtcSupportdGrpcServiceTest, RunUrandomRoutine) {
  std::unique_ptr<grpc_api::RunRoutineResponse> response;
  ExecuteRunRoutine(MakeRunUrandomRoutineRequest(), &response,
                    true /* is_valid_request */);
  auto expected_response = MakeRunRoutineResponse();
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test that a urandom routine with no parameters will fail.
TEST_F(WilcoDtcSupportdGrpcServiceTest, RunUrandomRoutineNoParameters) {
  std::unique_ptr<grpc_api::RunRoutineResponse> response;
  auto request = std::make_unique<grpc_api::RunRoutineRequest>();
  request->set_routine(grpc_api::ROUTINE_URANDOM);
  ExecuteRunRoutine(std::move(request), &response,
                    false /* is_valid_request */);
  EXPECT_EQ(response->uuid(), 0);
  EXPECT_EQ(response->status(), grpc_api::ROUTINE_STATUS_FAILED_TO_START);
}

TEST_F(WilcoDtcSupportdGrpcServiceTest, GetOsVersionUnset) {
  base::SysInfo::SetChromeOSVersionInfoForTest("", base::Time());
  std::string version;
  ExecuteGetOsVersion(&version);

  EXPECT_TRUE(version.empty());
}

TEST_F(WilcoDtcSupportdGrpcServiceTest, GetOsVersion) {
  constexpr char kLsbRelease[] = "CHROMEOS_RELEASE_VERSION=%s";
  constexpr char kOsVersion[] = "11932.0.2019_03_20_1100";

  base::SysInfo::SetChromeOSVersionInfoForTest(
      base::StringPrintf(kLsbRelease, kOsVersion), base::Time());
  std::string version;
  ExecuteGetOsVersion(&version);

  EXPECT_EQ(version, kOsVersion);
}

// Test that an empty string is a valid result.
TEST_F(WilcoDtcSupportdGrpcServiceTest, GetConfigurationDataEmpty) {
  std::unique_ptr<grpc_api::GetConfigurationDataResponse> response;
  ExecuteGetConfigurationData("", &response);
  EXPECT_EQ(response->json_configuration_data(), "");
}

TEST_F(WilcoDtcSupportdGrpcServiceTest, GetConfigurationData) {
  // The JSON configuration data is passed through from the cloud to DTC binary
  // and might not be in JSON format.
  constexpr char kFakeJsonConfigurationData[] = "Fake JSON Configuration Data";
  std::unique_ptr<grpc_api::GetConfigurationDataResponse> response;
  ExecuteGetConfigurationData(kFakeJsonConfigurationData, &response);
  EXPECT_EQ(response->json_configuration_data(), kFakeJsonConfigurationData);
}

TEST_F(WilcoDtcSupportdGrpcServiceTest, GetVpdFieldUnset) {
  grpc_api::GetVpdFieldResponse::Status status;
  std::string vpd_field_value;
  ASSERT_NO_FATAL_FAILURE(ExecuteGetVpdField(
      grpc_api::GetVpdFieldRequest::FIELD_UNSET, &status, &vpd_field_value));
  EXPECT_EQ(status,
            grpc_api::GetVpdFieldResponse::STATUS_ERROR_VPD_FIELD_UNKNOWN);
  EXPECT_TRUE(vpd_field_value.empty());
}

// Tests for the GetProcData() method of WilcoDtcSupportdGrpcServiceTest when a
// single file is requested.
//
// This is a parameterized test with the following parameters:
// * |proc_data_request_type| - type of the GetProcData() request to be executed
//   (see GetProcDataRequest::Type);
// * |relative_file_path| - relative path to the file which is expected to be
//    read by the executed GetProcData() request.
class SingleProcFileWilcoDtcSupportdGrpcServiceTest
    : public WilcoDtcSupportdGrpcServiceTest,
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

// Test that GetProcData() returns a single item with the requested file data
// when the file exists.
TEST_P(SingleProcFileWilcoDtcSupportdGrpcServiceTest, Basic) {
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
TEST_P(SingleProcFileWilcoDtcSupportdGrpcServiceTest, NonExisting) {
  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetProcData(proc_data_request_type(), &file_dumps);

  EXPECT_TRUE(file_dumps.empty())
      << "Obtained: "
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

INSTANTIATE_TEST_CASE_P(
    ,
    SingleProcFileWilcoDtcSupportdGrpcServiceTest,
    testing::Values(
        std::tie(grpc_api::GetProcDataRequest::FILE_UPTIME, "proc/uptime"),
        std::tie(grpc_api::GetProcDataRequest::FILE_MEMINFO, "proc/meminfo"),
        std::tie(grpc_api::GetProcDataRequest::FILE_LOADAVG, "proc/loadavg"),
        std::tie(grpc_api::GetProcDataRequest::FILE_STAT, "proc/stat"),
        std::tie(grpc_api::GetProcDataRequest::FILE_NET_NETSTAT,
                 "proc/net/netstat"),
        std::tie(grpc_api::GetProcDataRequest::FILE_NET_DEV, "proc/net/dev"),
        std::tie(grpc_api::GetProcDataRequest::FILE_DISKSTATS,
                 "proc/diskstats"),
        std::tie(grpc_api::GetProcDataRequest::FILE_CPUINFO, "proc/cpuinfo"),
        std::tie(grpc_api::GetProcDataRequest::FILE_VMSTAT, "proc/vmstat")));

// Tests for the GetSysfsData() method of WilcoDtcSupportdGrpcServiceTest when a
// directory is requested.
//
// This is a parameterized test with the following parameters:
// * |sysfs_data_request_type| - type of the GetSysfsData() request to be
//    executed (see GetSysfsDataRequest::Type);
// * |relative_dir_path| - relative path to the directory which is expected to
//    be read by the executed GetSysfsData() request.
class SysfsDirectoryWilcoDtcSupportdGrpcServiceTest
    : public WilcoDtcSupportdGrpcServiceTest,
      public testing::WithParamInterface<std::tuple<
          grpc_api::GetSysfsDataRequest::Type /* sysfs_data_request_type */,
          std::string /* relative_dir_path */,
          bool /* should_follow_symlink*/>> {
 protected:
  // Accessors to individual test parameters constructed from the test parameter
  // tuple returned by gtest's GetParam():

  grpc_api::GetSysfsDataRequest::Type sysfs_data_request_type() const {
    return std::get<0>(GetParam());
  }

  bool ShouldFollowSymlink() const { return std::get<2>(GetParam()); }

  base::FilePath relative_dir_path() const {
    return base::FilePath(std::get<1>(GetParam()));
  }

  base::FilePath absolute_dir_path() const {
    return temp_dir_path().Append(relative_dir_path());
  }

  base::FilePath relative_file_path() const {
    return relative_dir_path().Append(kRelativeFilePath);
  }

  base::FilePath absolute_file_path() const {
    return temp_dir_path().Append(relative_file_path());
  }

  base::FilePath relative_symlink_path() const {
    return relative_dir_path().Append(kRelativeSymlinkPath);
  }

  base::FilePath absolute_symlink_path() const {
    return temp_dir_path().Append(relative_symlink_path());
  }

  base::FilePath relative_nested_file_path() const {
    return relative_dir_path().Append(kRelativeNestedFilePath);
  }

  base::FilePath absolute_nested_file_path() const {
    return temp_dir_path().Append(relative_nested_file_path());
  }

 private:
  const std::string kRelativeFilePath = "foo_file";
  const std::string kRelativeSymlinkPath = "foo_symlink";
  const std::string kRelativeNestedFilePath = "foo_dir/nested_file";
};

// Test that GetSysfsData() returns an empty result when the directory doesn't
// exist.
TEST_P(SysfsDirectoryWilcoDtcSupportdGrpcServiceTest, NonExisting) {
  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetSysfsData(sysfs_data_request_type(), &file_dumps);

  EXPECT_TRUE(file_dumps.empty())
      << "Obtained: "
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

// Test that GetSysfsData() returns a single file when called on a directory
// containing a single file.
TEST_P(SysfsDirectoryWilcoDtcSupportdGrpcServiceTest, SingleFileInDirectory) {
  ASSERT_TRUE(
      WriteFileAndCreateParentDirs(absolute_file_path(), FakeFileContents()));

  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetSysfsData(sysfs_data_request_type(), &file_dumps);

  const auto kExpectedFileDump = MakeFileDump(
      relative_file_path(), relative_file_path(), FakeFileContents());
  EXPECT_THAT(file_dumps,
              UnorderedElementsAre(ProtobufEquals(kExpectedFileDump)))
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

// Test that GetSysfsData() returns an empty result when given a directory with
// only a cyclic symlink.
TEST_P(SysfsDirectoryWilcoDtcSupportdGrpcServiceTest, CyclicSymLink) {
  ASSERT_TRUE(CreateCyclicSymbolicLink(absolute_dir_path()));

  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetSysfsData(sysfs_data_request_type(), &file_dumps);

  EXPECT_TRUE(file_dumps.empty())
      << "Obtained: "
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

// Test that GetSysfsData() returns a single result when given a directory
// containing a file and a symlink to that same file.
TEST_P(SysfsDirectoryWilcoDtcSupportdGrpcServiceTest, DuplicateSymLink) {
  ASSERT_EQ(absolute_file_path().DirName(), absolute_symlink_path().DirName());
  ASSERT_TRUE(WriteFileAndCreateSymbolicLink(
      absolute_file_path(), FakeFileContents(), absolute_symlink_path()));

  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetSysfsData(sysfs_data_request_type(), &file_dumps);

  const auto kExpectedFileDump = MakeFileDump(
      relative_symlink_path(), relative_file_path(), FakeFileContents());
  ASSERT_EQ(file_dumps.size(), 1);
  // The non-canonical path could be either absolute_file_path() or
  // absolute_symlink_path(). Dumping a directory uses base::FileIterator,
  // whose order is not guaranteed.
  EXPECT_THAT(file_dumps[0].path(),
              AnyOf(StrEq(absolute_file_path().value()),
                    StrEq(absolute_symlink_path().value())));
  EXPECT_EQ(file_dumps[0].canonical_path(), absolute_file_path().value());
  EXPECT_EQ(file_dumps[0].contents(), FakeFileContents());
}

// Test that GetSysfsData() follows allowable symlinks.
TEST_P(SysfsDirectoryWilcoDtcSupportdGrpcServiceTest, ShouldFollowSymlink) {
  base::ScopedTempDir other_dir;
  ASSERT_TRUE(other_dir.CreateUniqueTempDir());
  base::FilePath file_path = other_dir.GetPath().Append("foo_file");
  ASSERT_TRUE(WriteFileAndCreateSymbolicLink(file_path, FakeFileContents(),
                                             absolute_symlink_path()));

  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetSysfsData(sysfs_data_request_type(), &file_dumps);

  if (ShouldFollowSymlink()) {
    grpc_api::FileDump expected_file_dump;
    expected_file_dump.set_path(absolute_symlink_path().value());
    expected_file_dump.set_canonical_path(file_path.value());
    expected_file_dump.set_contents(FakeFileContents());
    EXPECT_THAT(file_dumps,
                UnorderedElementsAre(ProtobufEquals(expected_file_dump)))
        << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
  } else {
    EXPECT_TRUE(file_dumps.empty())
        << "Obtained: "
        << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
  }
}

// Test that GetSysfsData returns correct file dumps for files in nested
// directories.
TEST_P(SysfsDirectoryWilcoDtcSupportdGrpcServiceTest,
       GetFileInNestedDirectory) {
  ASSERT_TRUE(WriteFileAndCreateParentDirs(absolute_nested_file_path(),
                                           FakeFileContents()));
  ASSERT_TRUE(WriteFileAndCreateParentDirs(absolute_file_path(),
                                           FakeSecondFileContents()));

  std::vector<grpc_api::FileDump> file_dumps;
  ExecuteGetSysfsData(sysfs_data_request_type(), &file_dumps);

  const auto kFirstExpectedFileDump =
      MakeFileDump(relative_nested_file_path(), relative_nested_file_path(),
                   FakeFileContents());
  const auto kSecondExpectedFileDump = MakeFileDump(
      relative_file_path(), relative_file_path(), FakeSecondFileContents());
  EXPECT_THAT(file_dumps,
              UnorderedElementsAre(ProtobufEquals(kFirstExpectedFileDump),
                                   ProtobufEquals(kSecondExpectedFileDump)))
      << GetProtosRangeDebugString(file_dumps.begin(), file_dumps.end());
}

INSTANTIATE_TEST_CASE_P(
    ,
    SysfsDirectoryWilcoDtcSupportdGrpcServiceTest,
    testing::Values(
        std::make_tuple(grpc_api::GetSysfsDataRequest::CLASS_HWMON,
                        "sys/class/hwmon/",
                        true),
        std::make_tuple(grpc_api::GetSysfsDataRequest::CLASS_THERMAL,
                        "sys/class/thermal/",
                        true),
        std::make_tuple(grpc_api::GetSysfsDataRequest::FIRMWARE_DMI_TABLES,
                        "sys/firmware/dmi/tables/",
                        false),
        std::make_tuple(grpc_api::GetSysfsDataRequest::CLASS_POWER_SUPPLY,
                        "sys/class/power_supply/",
                        true),
        std::make_tuple(grpc_api::GetSysfsDataRequest::CLASS_BACKLIGHT,
                        "sys/class/backlight/",
                        true),
        std::make_tuple(grpc_api::GetSysfsDataRequest::CLASS_NETWORK,
                        "sys/class/net/",
                        true),
        std::make_tuple(grpc_api::GetSysfsDataRequest::DEVICES_SYSTEM_CPU,
                        "sys/devices/system/cpu/",
                        false)));

// Tests for the GetEcTelemetry() method of WilcoDtcSupportdGrpcServiceTest.
//
// This is a parameterized test with the following parameters:
// * |request_payload| - payload of the GetEcTelemetry() request;
// * |expected_response_status| - expected GetEcTelemetry() response status;
// * |expected_response_payload| - expected GetEcTelemetry() response payload.
class GetEcTelemetryWilcoDtcSupportdGrpcServiceTest
    : public WilcoDtcSupportdGrpcServiceTest,
      public testing::WithParamInterface<
          std::tuple<std::string /* request_payload */,
                     grpc_api::GetEcTelemetryResponse::
                         Status /* expected_response_status */,
                     std::string /* expected_response_payload */>> {
 protected:
  std::string request_payload() const { return std::get<0>(GetParam()); }

  grpc_api::GetEcTelemetryResponse::Status expected_response_status() const {
    return std::get<1>(GetParam());
  }

  std::string expected_response_payload() const {
    return std::get<2>(GetParam());
  }

  base::FilePath devfs_telemetry_file() const {
    return temp_dir_path().Append(kEcGetTelemetryFilePath);
  }
};

// Test that GetEcTelemetry() response contains expected |status| and |payload|
// field values.
TEST_P(GetEcTelemetryWilcoDtcSupportdGrpcServiceTest, Base) {
  // Write request and response payload because EC telemetry char device is
  // non-seekable.
  EXPECT_TRUE(WriteFileAndCreateParentDirs(
      devfs_telemetry_file(), request_payload() + expected_response_payload()));
  std::unique_ptr<grpc_api::GetEcTelemetryResponse> response;
  ExecuteGetEcTelemetry(request_payload(), &response);
  ASSERT_TRUE(response);
  auto expected_response = MakeGetEcTelemetryResponse(
      expected_response_status(), expected_response_payload());
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

INSTANTIATE_TEST_CASE_P(
    ,
    GetEcTelemetryWilcoDtcSupportdGrpcServiceTest,
    testing::Values(
        std::make_tuple(FakeFileContents(),
                        grpc_api::GetEcTelemetryResponse::STATUS_OK,
                        FakeFileContents()),
        std::make_tuple(std::string("A", kEcGetTelemetryPayloadMaxSize),
                        grpc_api::GetEcTelemetryResponse::STATUS_OK,
                        std::string("B", kEcGetTelemetryPayloadMaxSize)),
        std::make_tuple(
            "",
            grpc_api::GetEcTelemetryResponse::STATUS_ERROR_INPUT_PAYLOAD_EMPTY,
            ""),
        std::make_tuple(std::string("A", kEcGetTelemetryPayloadMaxSize + 1),
                        grpc_api::GetEcTelemetryResponse::
                            STATUS_ERROR_INPUT_PAYLOAD_MAX_SIZE_EXCEEDED,
                        "")));

// Tests for the GetEcProperty() method of WilcoDtcSupportdGrpcServiceTest.
//
// This is a parameterized test with the following parameters:
// * |ec_property| - property of the GetEcProperty() request to be executed
//   (see GetEcPropertyRequest::Property);
// * |sysfs_file_name| - sysfs file in |kEcDriverSysfsPropertiesPath|
//   properties folder which is expected to be read by the executed
//   GetEcProperty() request.
class GetEcPropertyWilcoDtcSupportdGrpcServiceTest
    : public WilcoDtcSupportdGrpcServiceTest,
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

// Test that GetEcProperty() returns EC property value when appropriate
// sysfs file exists.
TEST_P(GetEcPropertyWilcoDtcSupportdGrpcServiceTest, SysfsFileExists) {
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
TEST_P(GetEcPropertyWilcoDtcSupportdGrpcServiceTest, SysfsFileDoesNotExist) {
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
    GetEcPropertyWilcoDtcSupportdGrpcServiceTest,
    testing::Values(
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_GLOBAL_MIC_MUTE_LED,
                 kEcPropertyGlobalMicMuteLed),
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_FN_LOCK,
                 kEcPropertyFnLock),
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_NIC, kEcPropertyNic),
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_EXT_USB_PORT_EN,
                 kEcPropertyExtUsbPortEn),
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_WIRELESS_SW_WLAN,
                 kEcPropertyWirelessSwWlan),
        std::tie(grpc_api::GetEcPropertyRequest::
                     PROPERTY_AUTO_BOOT_ON_TRINITY_DOCK_ATTACH,
                 kEcPropertyAutoBootOnTrinityDockAttach),
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_ICH_AZALIA_EN,
                 kEcPropertyIchAzaliaEn),
        std::tie(grpc_api::GetEcPropertyRequest::PROPERTY_SIGN_OF_LIFE_KBBL,
                 kEcPropertySignOfLifeKbbl)));

// Tests for the PerformWebRequest() method of WilcoDtcSupportdGrpcService.
//
// This is a parameterized test with the following parameters:
//
// The input arguments to create a PerformWebRequestParameter:
// * |http_method| - gRPC PerformWebRequest HTTP method.
// * |url| - gRPC PerformWebRequest URL.
// * |headers| - gRPC PerformWebRequest headers list.
// * |request_body| - gRPC PerformWebRequest request body.
//
// The intermediate parameters to verify by the test:
// * |delegate_http_method| - this is an optional value, a nullptr if the
//                            intermediate verification is not needed.
//                            WilcoDtcSupportdGrpcService's Delegate's HTTP
//                            method to verify the mapping between gRPC and
//                            Delegate's HTTP method names.
//
// The expected response values to verify PerformWebRequestResponse:
// * |status| - gRPC PerformWebRequestResponse status.
// * |http_status| - this is an optional value. gRPC PerformWebRequestResponse
//                   HTTP status. If there is no HTTP status needed for
//                   the passed |status|, pass a nullptr.
// * |response_body| - this is an optional value. gRPC PerformWebRequestResponse
//                     body. If not set, pass a nullptr.
class PerformWebRequestWilcoDtcSupportdGrpcServiceTest
    : public WilcoDtcSupportdGrpcServiceTest,
      public testing::WithParamInterface<
          std::tuple<grpc_api::PerformWebRequestParameter::HttpMethod,
                     std::string /* URL */,
                     std::vector<std::string> /* headers */,
                     std::string /* request body */,
                     const DelegateWebRequestHttpMethod*,
                     grpc_api::PerformWebRequestResponse::Status /* status */,
                     const int* /* HTTP status */,
                     const char* /* response body */>> {
 protected:
  grpc_api::PerformWebRequestParameter::HttpMethod http_method() {
    return std::get<0>(GetParam());
  }
  std::string url() const { return std::get<1>(GetParam()); }
  std::vector<std::string> headers() const { return std::get<2>(GetParam()); }
  std::string request_body() const { return std::get<3>(GetParam()); }
  const DelegateWebRequestHttpMethod* delegate_http_method() const {
    return std::get<4>(GetParam());
  }
  grpc_api::PerformWebRequestResponse::Status status() const {
    return std::get<5>(GetParam());
  }
  const int* http_status() const { return std::get<6>(GetParam()); }
  const char* response_body() const { return std::get<7>(GetParam()); }
};

// Tests that PerformWebRequest() returns an appropriate status and HTTP status
// code.
TEST_P(PerformWebRequestWilcoDtcSupportdGrpcServiceTest, PerformWebRequest) {
  std::unique_ptr<grpc_api::PerformWebRequestResponse> response;
  ExecutePerformWebRequest(http_method(), url(), headers(), request_body(),
                           delegate_http_method(), &response);
  ASSERT_TRUE(response);

  auto expected_response =
      MakePerformWebRequestResponse(status(), http_status(), response_body());
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test cases to run a PerformWebRequest test.
// Make sure that the delegate_http_header is not set if the flow does not
// involve the calls to WilcoDtcSupportdGrpcService::Delegate.
INSTANTIATE_TEST_CASE_P(
    ,
    PerformWebRequestWilcoDtcSupportdGrpcServiceTest,
    testing::Values(
        // Tests an incorrect HTTP method.
        std::make_tuple(grpc_api::PerformWebRequestParameter::HTTP_METHOD_UNSET,
                        kCorrectUrl,
                        std::vector<std::string>() /* headers */,
                        "" /* request_body */,
                        nullptr /* delegate_http_method */,
                        grpc_api::PerformWebRequestResponse ::
                            STATUS_ERROR_REQUIRED_FIELD_MISSING,
                        nullptr /* http_status */,
                        nullptr /* response_body */),
        // Tests an empty URL.
        std::make_tuple(
            grpc_api::PerformWebRequestParameter::HTTP_METHOD_GET,
            "" /* url */,
            std::vector<std::string>() /* headers */,
            "" /* request_body */,
            nullptr /* delegate_http_method */,
            grpc_api::PerformWebRequestResponse ::STATUS_ERROR_INVALID_URL,
            nullptr /* http_status */,
            nullptr /* response_body */),
        // Tests a non-HTTPS URL.
        std::make_tuple(
            grpc_api::PerformWebRequestParameter::HTTP_METHOD_PUT,
            kBadNonHttpsUrl,
            std::vector<std::string>() /* headers */,
            "" /* request_body */,
            nullptr /* delegate_http_method */,
            grpc_api::PerformWebRequestResponse::STATUS_ERROR_INVALID_URL,
            nullptr /* http_status */,
            nullptr /* response_body */),
        // Tests the maximum allowed number of headers with HTTP method GET.
        std::make_tuple(grpc_api::PerformWebRequestParameter::HTTP_METHOD_GET,
                        kCorrectUrl,
                        std::vector<std::string>(
                            kMaxNumberOfHeadersInPerformWebRequestParameter,
                            ""),
                        "" /* request_body */,
                        &kDelegateWebRequestHttpMethodGet,
                        grpc_api::PerformWebRequestResponse::STATUS_OK,
                        &kHttpStatusOk,
                        kFakeWebResponseBody),
        // The HTTP method is HEAD.
        std::make_tuple(grpc_api::PerformWebRequestParameter::HTTP_METHOD_HEAD,
                        kCorrectUrl,
                        std::vector<std::string>(
                            kMaxNumberOfHeadersInPerformWebRequestParameter,
                            ""),
                        "" /* request_body */,
                        &kDelegateWebRequestHttpMethodHead,
                        grpc_api::PerformWebRequestResponse::STATUS_OK,
                        &kHttpStatusOk,
                        kFakeWebResponseBody),
        // The HTTP method is POST.
        std::make_tuple(grpc_api::PerformWebRequestParameter::HTTP_METHOD_POST,
                        kCorrectUrl,
                        std::vector<std::string>() /* headers */,
                        "" /* request_body */,
                        &kDelegateWebRequestHttpMethodPost,
                        grpc_api::PerformWebRequestResponse::STATUS_OK,
                        &kHttpStatusOk,
                        kFakeWebResponseBody),
        // Tests the minimum not allowed number of headers.
        std::make_tuple(
            grpc_api::PerformWebRequestParameter::HTTP_METHOD_GET,
            kCorrectUrl,
            std::vector<std::string>(
                kMaxNumberOfHeadersInPerformWebRequestParameter + 1, ""),
            "" /* request_body */,
            nullptr /* delegate_http_method */,
            grpc_api::PerformWebRequestResponse::STATUS_ERROR_MAX_SIZE_EXCEEDED,
            nullptr /* http_status */,
            nullptr /* response_body */),
        // Tests the total size of "string" and "byte" fields of
        // PerformWebRequestParameter = 1Mb, the HTTP method is PUT.
        std::make_tuple(grpc_api::PerformWebRequestParameter::HTTP_METHOD_PUT,
                        kCorrectUrl,
                        std::vector<std::string>() /* headers */,
                        std::string(kMaxPerformWebRequestParameterSizeInBytes -
                                        strlen(kCorrectUrl),
                                    'A'),
                        &kDelegateWebRequestHttpMethodPut,
                        grpc_api::PerformWebRequestResponse::STATUS_OK,
                        &kHttpStatusOk,
                        kFakeWebResponseBody),
        // Tests the total size of "string" and "byte" fields of
        // PerformWebRequestParameter > 1Mb.
        std::make_tuple(
            grpc_api::PerformWebRequestParameter::HTTP_METHOD_GET,
            kCorrectUrl,
            std::vector<std::string>() /* headers */,
            std::string(kMaxPerformWebRequestParameterSizeInBytes, 'A'),
            nullptr /* delegate_http_method */,
            grpc_api::PerformWebRequestResponse::STATUS_ERROR_MAX_SIZE_EXCEEDED,
            nullptr /* http_status */,
            nullptr /* response_body */)));

// Tests for the GetRoutineUpdate() method of WilcoDtcSupportdGrpcService.
//
// This is a parameterized test with the following parameters:
//
// The input arguments to create a GetRoutineUpdateRequest:
// * |command| - gRPC GetRoutineUpdateRequest command.
class GetRoutineUpdateRequestWilcoDtcSupportdGrpcServiceTest
    : public WilcoDtcSupportdGrpcServiceTest,
      public testing::WithParamInterface<
          grpc_api::GetRoutineUpdateRequest::Command /* command */> {
 protected:
  grpc_api::GetRoutineUpdateRequest::Command command() const {
    return GetParam();
  }
};

// Tests that GetRoutineUpdate() returns an appropriate uuid, status, progress
// percent, user message and output.
TEST_P(GetRoutineUpdateRequestWilcoDtcSupportdGrpcServiceTest,
       GetRoutineUpdateRequestWithOutput) {
  std::unique_ptr<grpc_api::GetRoutineUpdateResponse> response;
  constexpr bool kIncludeOutput = true;
  ExecuteGetRoutineUpdate(kFakeUuid, command(), kIncludeOutput, &response);
  ASSERT_TRUE(response);

  auto expected_response =
      MakeGetRoutineUpdateResponse(kFakeUuid, kIncludeOutput);
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Tests that GetRoutineUpdate() does not return output when include_output is
// false.
TEST_P(GetRoutineUpdateRequestWilcoDtcSupportdGrpcServiceTest,
       GetRoutineUpdateRequestNoOutput) {
  std::unique_ptr<grpc_api::GetRoutineUpdateResponse> response;
  constexpr bool kIncludeOutput = false;
  ExecuteGetRoutineUpdate(kFakeUuid, command(), kIncludeOutput, &response);
  ASSERT_TRUE(response);

  auto expected_response =
      MakeGetRoutineUpdateResponse(kFakeUuid, kIncludeOutput);
  EXPECT_THAT(*response, ProtobufEquals(*expected_response))
      << "Actual response: {" << response->ShortDebugString() << "}";
}

// Test cases to run a GetRoutineUpdateRequest test.
INSTANTIATE_TEST_CASE_P(,
                        GetRoutineUpdateRequestWilcoDtcSupportdGrpcServiceTest,
                        testing::Values(
                            // Test each possible command value.
                            grpc_api::GetRoutineUpdateRequest::RESUME,
                            grpc_api::GetRoutineUpdateRequest::CANCEL,
                            grpc_api::GetRoutineUpdateRequest::GET_STATUS));

// Test for the GetVpdField() method of WilcoDtcSupportdGrpcService.
//
// This is a parametrized test with the following parameters:
// * |vpd_field| - the requested VPD field.
// * |file_path| - the corresponding file path.
// * |file_contents| - the fake VPD field value.
// * |expected_status| - the expected status.
// * |expected_value| - the expected value.
class GetVpdFieldWilcoDtcSupportdGrpcServiceTest
    : public WilcoDtcSupportdGrpcServiceTest,
      public testing::WithParamInterface<std::tuple<
          grpc_api::GetVpdFieldRequest::VpdField /* vpd_field */,
          std::string /* file_path */,
          std::string /* file_contents */,
          grpc_api::GetVpdFieldResponse::Status /* expected_status */,
          std::string /* expected_value */>> {
 protected:
  grpc_api::GetVpdFieldRequest::VpdField vpd_field() const {
    return std::get<0>(GetParam());
  }
  base::FilePath file_path() const {
    return temp_dir_path().Append(std::get<1>(GetParam()));
  }
  std::string file_contents() const { return std::get<2>(GetParam()); }
  grpc_api::GetVpdFieldResponse::Status expected_status() const {
    return std::get<3>(GetParam());
  }
  std::string expected_value() const { return std::get<4>(GetParam()); }
};

// Test that GetVpdField() is read properly.
TEST_P(GetVpdFieldWilcoDtcSupportdGrpcServiceTest, GetVpdField) {
  if (!file_contents().empty())
    EXPECT_TRUE(WriteFileAndCreateParentDirs(file_path(), file_contents()));
  grpc_api::GetVpdFieldResponse::Status status;
  std::string vpd_field_value;
  ASSERT_NO_FATAL_FAILURE(
      ExecuteGetVpdField(vpd_field(), &status, &vpd_field_value));

  EXPECT_EQ(status, expected_status());
  EXPECT_EQ(vpd_field_value, expected_value());
}

INSTANTIATE_TEST_CASE_P(
    ,
    GetVpdFieldWilcoDtcSupportdGrpcServiceTest,
    testing::Values(
        // Valid serial number test case.
        std::make_tuple(grpc_api::GetVpdFieldRequest::FIELD_SERIAL_NUMBER,
                        kVpdFieldSerialNumberFilePath,
                        kFakeSerialNumber,
                        grpc_api::GetVpdFieldResponse::STATUS_OK,
                        kFakeSerialNumber),
        // Valid serial number test case with whitespace characters.
        std::make_tuple(grpc_api::GetVpdFieldRequest::FIELD_SERIAL_NUMBER,
                        kVpdFieldSerialNumberFilePath,
                        base::StringPrintf("%s\n\t", kFakeSerialNumber),
                        grpc_api::GetVpdFieldResponse::STATUS_OK,
                        kFakeSerialNumber),
        // Empty serial number test case.
        std::make_tuple(grpc_api::GetVpdFieldRequest::FIELD_SERIAL_NUMBER,
                        kVpdFieldSerialNumberFilePath,
                        "\t\n " /* file_contents */,
                        grpc_api::GetVpdFieldResponse::STATUS_ERROR_INTERNAL,
                        "" /* expected_value */),
        // No file for serial number test case.
        std::make_tuple(grpc_api::GetVpdFieldRequest::FIELD_SERIAL_NUMBER,
                        "" /* file_path */,
                        "" /* file_contents */,
                        grpc_api::GetVpdFieldResponse::STATUS_ERROR_INTERNAL,
                        "" /* expected_value */)));

}  // namespace

}  // namespace diagnostics
