// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stddef.h>
#include <stdint.h>

#include <iostream>
#include <memory>

#include <base/at_exit.h>
#include <brillo/syslog_logging.h>
#include <libprotobuf-mutator/src/libfuzzer/libfuzzer_macro.h>
#include <gmock/gmock.h>

#include "vm_tools/cicerone/container_listener_impl.h"
#include "vm_tools/cicerone/service.h"
#include "vm_tools/cicerone/service_testing_helper.h"
#include "vm_tools/cicerone/tremplin_listener_impl.h"

#include "container_host.grpc.pb.h"  // NOLINT(build/include)
#include "fuzzer.pb.h"               // NOLINT(build/include)

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::vm_tools::cicerone::ContainerListenerImpl;
using ::vm_tools::cicerone::Service;
using ::vm_tools::cicerone::ServiceTestingHelper;

// Stuff to create & do once the first time the fuzzer runs.
struct SetupOnce {
  SetupOnce() {
    brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
    // Disable logging, as suggested in fuzzing instructions.
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }

  // Protobuf Mutator will create invalid protos sometimes (with duplicate
  // map keys). Silence those warnings as well.
  protobuf_mutator::protobuf::LogSilencer log_silencer_;

  base::AtExitManager at_exit_;
};

dbus::Response* ReturnDbusResponse() {
  // Will be owned by the caller; see comments in MockObjectProxy.
  return dbus::Response::CreateEmpty().release();
}

void SetUpMockObjectProxy(
    const vm_tools::container::ContainerListenerFuzzerSingleAction& action,
    dbus::MockObjectProxy* mock_object_proxy) {
  if (action.return_dbus_response()) {
    EXPECT_CALL(*mock_object_proxy, MockCallMethodAndBlock(_, _))
        .WillRepeatedly(InvokeWithoutArgs(&ReturnDbusResponse));
    EXPECT_CALL(*mock_object_proxy,
                MockCallMethodAndBlockWithErrorDetails(_, _, _))
        .WillRepeatedly(InvokeWithoutArgs(&ReturnDbusResponse));
  } else {
    EXPECT_CALL(*mock_object_proxy, MockCallMethodAndBlock(_, _))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_object_proxy,
                MockCallMethodAndBlockWithErrorDetails(_, _, _))
        .WillRepeatedly(Return(nullptr));
  }
}

grpc::Status ToStatus(int integer_status_code) {
  grpc::StatusCode status_code =
      static_cast<grpc::StatusCode>(integer_status_code);
  grpc::Status status(status_code, "");
  return status;
}

void SetUpTremplinTestStub(
    const vm_tools::container::ContainerListenerFuzzerSingleAction& action,
    vm_tools::cicerone::TremplinTestStub* test_stub) {
  test_stub->SetCreateContainerReturn(
      ToStatus(action.tremplin_create_container_status()));
  test_stub->SetCreateContainerResponse(
      action.tremplin_create_container_response());
  test_stub->SetStartContainerReturn(
      ToStatus(action.tremplin_start_container_status()));
  test_stub->SetStartContainerResponse(
      action.tremplin_start_container_response());
  test_stub->SetGetContainerUsernameReturn(
      ToStatus(action.tremplin_get_container_username_status()));
  test_stub->SetGetContainerUsernameResponse(
      action.tremplin_get_container_username_response());
  test_stub->SetSetUpUserReturn(ToStatus(action.tremplin_set_up_user_status()));
  test_stub->SetSetUpUserResponse(action.tremplin_set_up_user_response());
  test_stub->SetGetContainerInfoReturn(
      ToStatus(action.tremplin_get_container_info_status()));
  test_stub->SetGetContainerInfoResponse(
      action.tremplin_get_container_info_response());
  test_stub->SetSetTimezoneReturn(
      ToStatus(action.tremplin_set_timezone_status()));
  test_stub->SetSetTimezoneResponse(action.tremplin_set_timezone_response());
  test_stub->SetExportContainerReturn(
      ToStatus(action.tremplin_export_container_status()));
  test_stub->SetExportContainerResponse(
      action.tremplin_export_container_response());
  test_stub->SetImportContainerReturn(
      ToStatus(action.tremplin_import_container_status()));
  test_stub->SetImportContainerResponse(
      action.tremplin_import_container_response());
}

}  // namespace

DEFINE_PROTO_FUZZER(
    const vm_tools::container::ContainerListenerFuzzerInput& input) {
  static SetupOnce* setup_once = new SetupOnce;
  // Get rid of the unused variable warning.
  (void)setup_once;

  // We create the ServiceTestingHelper here, not in the static SetupOnce. This
  // is to force the threads to finish up before exiting this function --
  // destructing Service will force its threads to exit.
  ServiceTestingHelper test_framework(ServiceTestingHelper::NICE_MOCKS);
  test_framework.SetUpDefaultVmAndContainer();

  for (const vm_tools::container::ContainerListenerFuzzerSingleAction& action :
       input.action()) {
    ContainerListenerImpl* container_listener =
        test_framework.get_service().GetContainerListenerImpl();
    container_listener->OverridePeerAddressForTesting(action.peer_address());
    test_framework.get_service()
        .GetTremplinListenerImpl()
        ->OverridePeerAddressForTesting(action.peer_address());

    SetUpMockObjectProxy(
        action, &test_framework.get_mock_vm_applications_service_proxy());
    SetUpMockObjectProxy(action,
                         &test_framework.get_mock_url_handler_service_proxy());
    SetUpMockObjectProxy(action,
                         &test_framework.get_mock_crosdns_service_proxy());
    SetUpMockObjectProxy(action,
                         &test_framework.get_mock_concierge_service_proxy());

    SetUpTremplinTestStub(action, &test_framework.get_tremplin_test_stub());

    grpc::ServerContext context;
    vm_tools::EmptyMessage response;

    switch (action.input_case()) {
      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kContainerStartupInfo:
        container_listener->ContainerReady(
            &context, &action.container_startup_info(), &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kContainerShutdownInfo:
        container_listener->ContainerShutdown(
            &context, &action.container_shutdown_info(), &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kUpdateApplicationListRequest:
        container_listener->UpdateApplicationList(
            &context, &action.update_application_list_request(), &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kOpenUrlRequest:
        container_listener->OpenUrl(&context, &action.open_url_request(),
                                    &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kInstallLinuxPackageProgressInfo:
        container_listener->InstallLinuxPackageProgress(
            &context, &action.install_linux_package_progress_info(), &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kUninstallPackageProgressInfo:
        container_listener->UninstallPackageProgress(
            &context, &action.uninstall_package_progress_info(), &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kOpenTerminalRequest:
        container_listener->OpenTerminal(
            &context, &action.open_terminal_request(), &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kUpdateMimeTypesRequest:
        container_listener->UpdateMimeTypes(
            &context, &action.update_mime_types_request(), &response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          INPUT_NOT_SET:
        break;

      default:
        NOTREACHED();
    }
  }
}
