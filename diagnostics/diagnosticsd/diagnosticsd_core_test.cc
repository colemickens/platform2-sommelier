// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/stringprintf.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <dbus/diagnosticsd/dbus-constants.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/property.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/edk/embedder/embedder.h>

#include "diagnostics/diagnosticsd/diagnosticsd_core.h"
#include "diagnostics/diagnosticsd/fake_browser.h"
#include "diagnostics/diagnosticsd/fake_diagnostics_processor.h"
#include "diagnostics/diagnosticsd/mojo_test_utils.h"

#include "mojo/diagnosticsd.mojom.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArg;

namespace diagnostics {

// Templates for the gRPC URIs that should be used for testing. "%s" is
// substituted with a temporary directory.
const char kDiagnosticsdGrpcUriTemplate[] = "unix:%s/test_diagnosticsd_socket";
const char kDiagnosticsProcessorGrpcUriTemplate[] =
    "unix:%s/test_diagnostics_processor_socket";

using MojomDiagnosticsdService =
    chromeos::diagnostics::mojom::DiagnosticsdService;

namespace {

class MockDiagnosticsdCoreDelegate : public DiagnosticsdCore::Delegate {
 public:
  std::unique_ptr<mojo::Binding<MojomDiagnosticsdService>>
  BindDiagnosticsdMojoService(MojomDiagnosticsdService* mojo_service,
                              base::ScopedFD mojo_pipe_fd) override {
    // Redirect to a separate mockable method to workaround GMock's issues with
    // move-only return values.
    return std::unique_ptr<mojo::Binding<MojomDiagnosticsdService>>(
        BindDiagnosticsdMojoServiceImpl(mojo_service, mojo_pipe_fd.get()));
  }

  MOCK_METHOD2(BindDiagnosticsdMojoServiceImpl,
               mojo::Binding<MojomDiagnosticsdService>*(
                   MojomDiagnosticsdService* mojo_service, int mojo_pipe_fd));
  MOCK_METHOD0(BeginDaemonShutdown, void());
};

// Tests for the DiagnosticsdCore class.
class DiagnosticsdCoreTest : public testing::Test {
 protected:
  DiagnosticsdCoreTest() { InitializeMojo(); }

  ~DiagnosticsdCoreTest() { SetDBusShutdownExpectations(); }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    const std::string grpc_service_uri = base::StringPrintf(
        kDiagnosticsdGrpcUriTemplate, temp_dir_.GetPath().value().c_str());
    diagnostics_processor_grpc_uri_ =
        base::StringPrintf(kDiagnosticsProcessorGrpcUriTemplate,
                           temp_dir_.GetPath().value().c_str());
    core_ = std::make_unique<DiagnosticsdCore>(
        grpc_service_uri, diagnostics_processor_grpc_uri_, &core_delegate_);
    ASSERT_TRUE(core_->StartGrpcCommunication());

    SetUpDBus();

    fake_browser_.reset(new FakeBrowser(
        &mojo_service_interface_ptr_, bootstrap_mojo_connection_dbus_method_));
  }

  void TearDown() override {
    base::RunLoop run_loop;
    core_->TearDownGrpcCommunication(run_loop.QuitClosure());
    run_loop.Run();
  }

  MockDiagnosticsdCoreDelegate* core_delegate() { return &core_delegate_; }

  mojo::InterfacePtr<MojomDiagnosticsdService>* mojo_service_interface_ptr() {
    return &mojo_service_interface_ptr_;
  }

  FakeBrowser* fake_browser() { return fake_browser_.get(); }

  // Set up mock for BindDiagnosticsdMojoService() that simulates successful
  // Mojo service binding to the given file descriptor. After the mock gets
  // triggered, |mojo_service_interface_ptr_| become initialized to point to the
  // tested Mojo service.
  void SetSuccessMockBindDiagnosticsdMojoService(
      FakeMojoFdGenerator* fake_mojo_fd_generator) {
    EXPECT_CALL(core_delegate_, BindDiagnosticsdMojoServiceImpl(_, _))
        .WillOnce(Invoke(
            [fake_mojo_fd_generator, this](
                MojomDiagnosticsdService* mojo_service, int mojo_pipe_fd) {
              // Verify the file descriptor is a duplicate of an expected one.
              EXPECT_TRUE(fake_mojo_fd_generator->IsDuplicateFd(mojo_pipe_fd));
              // Initialize a Mojo binding that, instead of working through the
              // given (fake) file descriptor, talks to the test endpoint
              // |mojo_service_interface_ptr_|.
              auto mojo_service_binding =
                  std::make_unique<mojo::Binding<MojomDiagnosticsdService>>(
                      mojo_service, &mojo_service_interface_ptr_);
              DCHECK(mojo_service_interface_ptr_);
              return mojo_service_binding.release();
            }));
  }

  dbus::ExportedObject::MethodCallCallback
  bootstrap_mojo_connection_dbus_method() {
    return bootstrap_mojo_connection_dbus_method_;
  }

  const std::string& diagnostics_processor_grpc_uri() const {
    return diagnostics_processor_grpc_uri_;
  }

 private:
  // Initialize the Mojo subsystem.
  void InitializeMojo() { mojo::edk::Init(); }

  // Perform initialization of the D-Bus object exposed by the tested code.
  void SetUpDBus() {
    const dbus::ObjectPath kDBusObjectPath(kDiagnosticsdServicePath);

    // Expect that the /org/chromium/Diagnosticsd object is exported.
    diagnosticsd_dbus_object_ = new StrictMock<dbus::MockExportedObject>(
        dbus_bus_.get(), kDBusObjectPath);
    EXPECT_CALL(*dbus_bus_, GetExportedObject(kDBusObjectPath))
        .WillOnce(Return(diagnosticsd_dbus_object_.get()));

    // Expect that standard methods on the org.freedesktop.DBus.Properties
    // interface are exported.
    EXPECT_CALL(
        *diagnosticsd_dbus_object_,
        ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesGet, _, _));
    EXPECT_CALL(
        *diagnosticsd_dbus_object_,
        ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesSet, _, _));
    EXPECT_CALL(*diagnosticsd_dbus_object_,
                ExportMethod(dbus::kPropertiesInterface,
                             dbus::kPropertiesGetAll, _, _));

    // Expect that methods on the org.chromium.DiagnosticsdInterface interface
    // are exported.
    EXPECT_CALL(*diagnosticsd_dbus_object_,
                ExportMethod(kDiagnosticsdServiceInterface,
                             kDiagnosticsdBootstrapMojoConnectionMethod, _, _))
        .WillOnce(SaveArg<2 /* method_call_callback */>(
            &bootstrap_mojo_connection_dbus_method_));

    // Run the tested code that exports D-Bus objects and methods.
    scoped_refptr<brillo::dbus_utils::AsyncEventSequencer> dbus_sequencer(
        new brillo::dbus_utils::AsyncEventSequencer());
    core_->RegisterDBusObjectsAsync(dbus_bus_, dbus_sequencer.get());

    // Verify that required D-Bus methods are exported.
    EXPECT_FALSE(bootstrap_mojo_connection_dbus_method_.is_null());
  }

  // Set mock expectations for calls triggered during test destruction.
  void SetDBusShutdownExpectations() {
    EXPECT_CALL(*diagnosticsd_dbus_object_, Unregister());
  }

  base::MessageLoop message_loop_;

  base::ScopedTempDir temp_dir_;

  std::string diagnostics_processor_grpc_uri_;

  scoped_refptr<StrictMock<dbus::MockBus>> dbus_bus_ =
      new StrictMock<dbus::MockBus>(dbus::Bus::Options());

  // Mock D-Bus integration helper for the object exposed by the tested code.
  scoped_refptr<StrictMock<dbus::MockExportedObject>> diagnosticsd_dbus_object_;

  // Mojo interface to the service exposed by the tested code.
  mojo::InterfacePtr<MojomDiagnosticsdService> mojo_service_interface_ptr_;

  StrictMock<MockDiagnosticsdCoreDelegate> core_delegate_;

  std::unique_ptr<DiagnosticsdCore> core_;

  // Callback that the tested code exposed as the BootstrapMojoConnection D-Bus
  // method.
  dbus::ExportedObject::MethodCallCallback
      bootstrap_mojo_connection_dbus_method_;

  std::unique_ptr<FakeBrowser> fake_browser_;
};

}  // namespace

// Test that the Mojo service gets successfully bootstrapped after the
// BootstrapMojoConnection D-Bus method is called.
TEST_F(DiagnosticsdCoreTest, MojoBootstrapSuccess) {
  FakeMojoFdGenerator fake_mojo_fd_generator;
  SetSuccessMockBindDiagnosticsdMojoService(&fake_mojo_fd_generator);

  EXPECT_TRUE(fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  EXPECT_TRUE(*mojo_service_interface_ptr());
}

// Test failure to bootstrap the Mojo service due to en error returned by
// BindDiagnosticsdMojoService() delegate method.
TEST_F(DiagnosticsdCoreTest, MojoBootstrapErrorToBind) {
  FakeMojoFdGenerator fake_mojo_fd_generator;
  EXPECT_CALL(*core_delegate(), BindDiagnosticsdMojoServiceImpl(_, _))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*core_delegate(), BeginDaemonShutdown());

  EXPECT_FALSE(
      fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  Mock::VerifyAndClearExpectations(core_delegate());
}

// Test that second attempt to bootstrap the Mojo service results in error and
// the daemon shutdown.
TEST_F(DiagnosticsdCoreTest, MojoBootstrapErrorRepeated) {
  FakeMojoFdGenerator first_fake_mojo_fd_generator;
  SetSuccessMockBindDiagnosticsdMojoService(&first_fake_mojo_fd_generator);

  EXPECT_TRUE(
      fake_browser()->BootstrapMojoConnection(&first_fake_mojo_fd_generator));
  Mock::VerifyAndClearExpectations(core_delegate());

  FakeMojoFdGenerator second_fake_mojo_fd_generator;
  EXPECT_CALL(*core_delegate(), BeginDaemonShutdown());

  EXPECT_FALSE(
      fake_browser()->BootstrapMojoConnection(&second_fake_mojo_fd_generator));

  Mock::VerifyAndClearExpectations(core_delegate());
}

// Test that the daemon gets shut down when the previously bootstrapped Mojo
// connection aborts.
TEST_F(DiagnosticsdCoreTest, MojoBootstrapSuccessThenAbort) {
  FakeMojoFdGenerator fake_mojo_fd_generator;
  SetSuccessMockBindDiagnosticsdMojoService(&fake_mojo_fd_generator);

  EXPECT_TRUE(fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  Mock::VerifyAndClearExpectations(core_delegate());

  EXPECT_CALL(*core_delegate(), BeginDaemonShutdown());

  // Abort the Mojo connection by closing the |mojo_service_interface_ptr()|
  // endpoint.
  mojo_service_interface_ptr()->reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(core_delegate());
}

// TODO(crbug.com/893756): Causing failures on pre-cq due to unavailability of
// /dev/shm.
// Test that diagnostics processor will receive message from browser.
TEST_F(DiagnosticsdCoreTest, DISABLED_SendGrpcUiMessageToDiagnosticsProcessor) {
  const std::string json_message = "{\"some_key\": \"some_value\"}";

  base::RunLoop run_loop_handle_message;
  FakeDiagnosticsProcessor fake_diagnostics_processor(
      diagnostics_processor_grpc_uri());
  fake_diagnostics_processor.set_handle_message_from_ui_callback(
      run_loop_handle_message.QuitClosure());
  fake_diagnostics_processor.Start();

  FakeMojoFdGenerator fake_mojo_fd_generator;
  SetSuccessMockBindDiagnosticsdMojoService(&fake_mojo_fd_generator);
  EXPECT_TRUE(fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  EXPECT_TRUE(*mojo_service_interface_ptr());

  EXPECT_TRUE(fake_browser()->SendMessageToDiagnosticsProcessor(json_message));

  run_loop_handle_message.Run();
  EXPECT_EQ(json_message, fake_diagnostics_processor
                              .handle_message_from_ui_actual_json_message()
                              .value());

  base::RunLoop run_loop_shutdown;
  fake_diagnostics_processor.Shutdown(run_loop_shutdown.QuitClosure());
  run_loop_shutdown.Run();
}

// TODO(crbug.com/893756): Causing failures on pre-cq due to unavailability of
// /dev/shm.
// Test that diagnostics processor will not receive message from browser
// if JSON message is invalid.
TEST_F(DiagnosticsdCoreTest,
       DISABLED_SendGrpcUiMessageToDiagnosticsProcessorInvalidJSON) {
  const std::string json_message = "{'some_key': 'some_value'}";

  FakeDiagnosticsProcessor fake_diagnostics_processor(
      diagnostics_processor_grpc_uri());
  fake_diagnostics_processor.Start();

  FakeMojoFdGenerator fake_mojo_fd_generator;
  SetSuccessMockBindDiagnosticsdMojoService(&fake_mojo_fd_generator);
  EXPECT_TRUE(fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  EXPECT_TRUE(*mojo_service_interface_ptr());

  EXPECT_TRUE(fake_browser()->SendMessageToDiagnosticsProcessor(json_message));

  base::RunLoop run_loop_shutdown;
  fake_diagnostics_processor.Shutdown(run_loop_shutdown.QuitClosure());
  run_loop_shutdown.Run();

  EXPECT_FALSE(
      fake_diagnostics_processor.handle_message_from_ui_actual_json_message()
          .has_value());
}

}  // namespace diagnostics
