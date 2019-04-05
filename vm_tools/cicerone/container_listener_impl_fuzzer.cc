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
#include "vm_tools/cicerone/mock_tremplin_stub.h"
#include "vm_tools/cicerone/service.h"
#include "vm_tools/cicerone/service_testing_helper.h"
#include "vm_tools/cicerone/tremplin_listener_impl.h"

#include "container_host.grpc.pb.h"  // NOLINT(build/include)
#include "fuzzer.pb.h"               // NOLINT(build/include)

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::vm_tools::cicerone::ContainerListenerImpl;
using ::vm_tools::cicerone::Service;
using ::vm_tools::cicerone::ServiceTestingHelper;
using ::vm_tools::cicerone::TremplinListenerImpl;

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

std::unique_ptr<vm_tools::tremplin::MockTremplinStub> CreateMockTremplinStub(
    const vm_tools::container::ContainerListenerFuzzerSingleAction& action) {
  auto mock_tremplin_stub =
      std::make_unique<vm_tools::tremplin::MockTremplinStub>();

  EXPECT_CALL(*mock_tremplin_stub, CreateContainer(_, _, _))
      .Times(AnyNumber())
      .WillOnce(
          DoAll(SetArgPointee<2>(action.tremplin_create_container_response()),
                Return(ToStatus(action.tremplin_create_container_status()))));
  EXPECT_CALL(*mock_tremplin_stub, StartContainer(_, _, _))
      .Times(AnyNumber())
      .WillOnce(
          DoAll(SetArgPointee<2>(action.tremplin_start_container_response()),
                Return(ToStatus(action.tremplin_start_container_status()))));
  EXPECT_CALL(*mock_tremplin_stub, GetContainerUsername(_, _, _))
      .Times(AnyNumber())
      .WillOnce(DoAll(
          SetArgPointee<2>(action.tremplin_get_container_username_response()),
          Return(ToStatus(action.tremplin_get_container_username_status()))));
  EXPECT_CALL(*mock_tremplin_stub, SetUpUser(_, _, _))
      .Times(AnyNumber())
      .WillOnce(DoAll(SetArgPointee<2>(action.tremplin_set_up_user_response()),
                      Return(ToStatus(action.tremplin_set_up_user_status()))));
  EXPECT_CALL(*mock_tremplin_stub, GetContainerInfo(_, _, _))
      .Times(AnyNumber())
      .WillOnce(
          DoAll(SetArgPointee<2>(action.tremplin_get_container_info_response()),
                Return(ToStatus(action.tremplin_get_container_info_status()))));
  EXPECT_CALL(*mock_tremplin_stub, SetTimezone(_, _, _))
      .Times(AnyNumber())
      .WillOnce(DoAll(SetArgPointee<2>(action.tremplin_set_timezone_response()),
                      Return(ToStatus(action.tremplin_set_timezone_status()))));
  EXPECT_CALL(*mock_tremplin_stub, ExportContainer(_, _, _))
      .Times(AnyNumber())
      .WillOnce(
          DoAll(SetArgPointee<2>(action.tremplin_export_container_response()),
                Return(ToStatus(action.tremplin_export_container_status()))));
  EXPECT_CALL(*mock_tremplin_stub, ImportContainer(_, _, _))
      .Times(AnyNumber())
      .WillOnce(
          DoAll(SetArgPointee<2>(action.tremplin_import_container_response()),
                Return(ToStatus(action.tremplin_import_container_status()))));

  return mock_tremplin_stub;
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
    TremplinListenerImpl* tremplin_listener =
        test_framework.get_service().GetTremplinListenerImpl();
    tremplin_listener->OverridePeerAddressForTesting(action.peer_address());

    SetUpMockObjectProxy(
        action, &test_framework.get_mock_vm_applications_service_proxy());
    SetUpMockObjectProxy(action,
                         &test_framework.get_mock_url_handler_service_proxy());
    SetUpMockObjectProxy(action,
                         &test_framework.get_mock_crosdns_service_proxy());
    SetUpMockObjectProxy(action,
                         &test_framework.get_mock_concierge_service_proxy());

    test_framework.SetTremplinStub(ServiceTestingHelper::kDefaultOwnerId,
                                   ServiceTestingHelper::kDefaultVmName,
                                   CreateMockTremplinStub(action));

    grpc::ServerContext context;
    vm_tools::EmptyMessage response;
    vm_tools::tremplin::EmptyMessage tremplin_response;

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
          kTremplinStartupInfo:
        tremplin_listener->TremplinReady(
            &context, &action.tremplin_startup_info(), &tremplin_response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kContainerCreationProgress:
        tremplin_listener->UpdateCreateStatus(
            &context, &action.container_creation_progress(),
            &tremplin_response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kContainerDeletionProgress:
        tremplin_listener->UpdateDeletionStatus(
            &context, &action.container_deletion_progress(),
            &tremplin_response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kContainerStartProgress:
        tremplin_listener->UpdateStartStatus(
            &context, &action.container_start_progress(), &tremplin_response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kContainerExportProgress:
        tremplin_listener->UpdateExportStatus(
            &context, &action.container_export_progress(), &tremplin_response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kContainerImportProgress:
        tremplin_listener->UpdateImportStatus(
            &context, &action.container_import_progress(), &tremplin_response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          kTremplinContainerShutdownInfo:
        tremplin_listener->ContainerShutdown(
            &context, &action.tremplin_container_shutdown_info(),
            &tremplin_response);
        break;

      case vm_tools::container::ContainerListenerFuzzerSingleAction::
          INPUT_NOT_SET:
        break;

      default:
        NOTREACHED();
    }
  }
}
