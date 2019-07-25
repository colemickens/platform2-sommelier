// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/shared_memory.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/stringprintf.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <dbus/wilco_dtc_supportd/dbus-constants.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/property.h>
#include <gmock/gmock.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <mojo/public/cpp/bindings/interface_ptr.h>

#include "diagnostics/common/file_test_utils.h"
#include "diagnostics/common/mojo_test_utils.h"
#include "diagnostics/common/mojo_utils.h"
#include "diagnostics/wilco_dtc_supportd/bind_utils.h"
#include "diagnostics/wilco_dtc_supportd/ec_constants.h"
#include "diagnostics/wilco_dtc_supportd/fake_browser.h"
#include "diagnostics/wilco_dtc_supportd/fake_wilco_dtc.h"
#include "diagnostics/wilco_dtc_supportd/protobuf_test_utils.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_core.h"
#include "mojo/wilco_dtc_supportd.mojom.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace diagnostics {

// Templates for the gRPC URIs that should be used for testing. "%s" is
// substituted with a temporary directory.
const char kWilcoDtcSupportdGrpcUriTemplate[] =
    "unix:%s/test_wilco_dtc_supportd_socket";
const char kWilcoDtcGrpcUriTemplate[] = "unix:%s/test_wilco_dtc_socket";
const char kUiMessageReceiverWilcoDtcGrpcUriTemplate[] =
    "unix:%s/test_ui_message_receiver_wilco_dtc_socket";

using EcEvent = WilcoDtcSupportdEcEventService::EcEvent;
using MojomWilcoDtcSupportdService =
    chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdService;
using MojomWilcoDtcSupportdServiceFactory =
    chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory;

namespace {

// Returns a callback that, once called, saves its parameter to |*response| and
// quits |*run_loop|.
template <typename ValueType>
base::Callback<void(std::unique_ptr<ValueType>)> MakeAsyncResponseWriter(
    std::unique_ptr<ValueType>* response, base::RunLoop* run_loop) {
  return base::Bind(
      [](std::unique_ptr<ValueType>* response, base::RunLoop* run_loop,
         std::unique_ptr<ValueType> received_response) {
        EXPECT_TRUE(received_response);
        EXPECT_FALSE(*response);
        *response = std::move(received_response);
        run_loop->Quit();
      },
      base::Unretained(response), base::Unretained(run_loop));
}

class MockWilcoDtcSupportdCoreDelegate : public WilcoDtcSupportdCore::Delegate {
 public:
  std::unique_ptr<mojo::Binding<MojomWilcoDtcSupportdServiceFactory>>
  BindWilcoDtcSupportdMojoServiceFactory(
      MojomWilcoDtcSupportdServiceFactory* mojo_service_factory,
      base::ScopedFD mojo_pipe_fd) override {
    // Redirect to a separate mockable method to workaround GMock's issues with
    // move-only types.
    return std::unique_ptr<mojo::Binding<MojomWilcoDtcSupportdServiceFactory>>(
        BindWilcoDtcSupportdMojoServiceFactoryImpl(mojo_service_factory,
                                                   mojo_pipe_fd.get()));
  }

  MOCK_METHOD2(BindWilcoDtcSupportdMojoServiceFactoryImpl,
               mojo::Binding<MojomWilcoDtcSupportdServiceFactory>*(
                   MojomWilcoDtcSupportdServiceFactory* mojo_service_factory,
                   int mojo_pipe_fd));
  MOCK_METHOD0(BeginDaemonShutdown, void());
};

// Tests for the WilcoDtcSupportdCore class.
class WilcoDtcSupportdCoreTest : public testing::Test {
 protected:
  WilcoDtcSupportdCoreTest() { InitializeMojo(); }

  void CreateCore(const std::vector<std::string>& grpc_service_uris,
                  const std::string& ui_message_receiver_wilco_dtc_grpc_uri,
                  const std::vector<std::string>& wilco_dtc_grpc_uris) {
    core_ = std::make_unique<WilcoDtcSupportdCore>(
        grpc_service_uris, ui_message_receiver_wilco_dtc_grpc_uri,
        wilco_dtc_grpc_uris, &core_delegate_);
  }

  WilcoDtcSupportdCore* core() {
    DCHECK(core_);
    return core_.get();
  }

  MockWilcoDtcSupportdCoreDelegate* core_delegate() { return &core_delegate_; }

 private:
  // Initialize the Mojo subsystem.
  void InitializeMojo() { mojo::edk::Init(); }

  base::MessageLoop message_loop_;

  StrictMock<MockWilcoDtcSupportdCoreDelegate> core_delegate_;

  std::unique_ptr<WilcoDtcSupportdCore> core_;
};

}  // namespace

// Test successful shutdown after failed start.
TEST_F(WilcoDtcSupportdCoreTest, FailedStartAndSuccessfulShutdown) {
  // Invalid gRPC service URI.
  CreateCore({""}, "", {""});
  EXPECT_FALSE(core()->Start());

  base::RunLoop run_loop;
  core()->ShutDown(run_loop.QuitClosure());
  run_loop.Run();
}

namespace {

// Tests for the WilcoDtcSupportdCore class which started successfully.
class StartedWilcoDtcSupportdCoreTest : public WilcoDtcSupportdCoreTest {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(WilcoDtcSupportdCoreTest::SetUp());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    wilco_dtc_supportd_grpc_uri_ = base::StringPrintf(
        kWilcoDtcSupportdGrpcUriTemplate, temp_dir_.GetPath().value().c_str());
    ui_message_receiver_wilco_dtc_grpc_uri_ =
        base::StringPrintf(kUiMessageReceiverWilcoDtcGrpcUriTemplate,
                           temp_dir_.GetPath().value().c_str());

    wilco_dtc_grpc_uri_ = base::StringPrintf(
        kWilcoDtcGrpcUriTemplate, temp_dir_.GetPath().value().c_str());

    CreateCore({wilco_dtc_supportd_grpc_uri_},
               ui_message_receiver_wilco_dtc_grpc_uri_, {wilco_dtc_grpc_uri_});
    core()->set_root_dir_for_testing(temp_dir_.GetPath());

    SetUpEcEventService();

    ASSERT_TRUE(core()->Start());

    SetUpEcEventServiceFifoWriteEnd();

    SetUpDBus();

    fake_browser_ =
        std::make_unique<FakeBrowser>(&mojo_service_factory_interface_ptr_,
                                      bootstrap_mojo_connection_dbus_method_);
  }

  void TearDown() override {
    SetDBusShutdownExpectations();

    base::RunLoop run_loop;
    core()->ShutDown(run_loop.QuitClosure());
    run_loop.Run();

    WilcoDtcSupportdCoreTest::TearDown();
  }

  const base::FilePath& temp_dir_path() const {
    DCHECK(temp_dir_.IsValid());
    return temp_dir_.GetPath();
  }

  mojo::InterfacePtr<MojomWilcoDtcSupportdServiceFactory>*
  mojo_service_factory_interface_ptr() {
    return &mojo_service_factory_interface_ptr_;
  }

  FakeBrowser* fake_browser() {
    DCHECK(fake_browser_);
    return fake_browser_.get();
  }

  // Set up mock for BindWilcoDtcSupportdMojoServiceFactory() that simulates
  // successful Mojo service binding to the given file descriptor. After the
  // mock gets triggered, |mojo_service_factory_interface_ptr_| become
  // initialized to point to the tested Mojo service.
  void SetSuccessMockBindWilcoDtcSupportdMojoService(
      FakeMojoFdGenerator* fake_mojo_fd_generator) {
    EXPECT_CALL(*core_delegate(),
                BindWilcoDtcSupportdMojoServiceFactoryImpl(_, _))
        .WillOnce(Invoke(
            [fake_mojo_fd_generator, this](
                MojomWilcoDtcSupportdServiceFactory* mojo_service_factory,
                int mojo_pipe_fd) {
              // Verify the file descriptor is a duplicate of an expected one.
              EXPECT_TRUE(fake_mojo_fd_generator->IsDuplicateFd(mojo_pipe_fd));
              // Initialize a Mojo binding that, instead of working through the
              // given (fake) file descriptor, talks to the test endpoint
              // |mojo_service_interface_ptr_|.
              auto mojo_service_factory_binding = std::make_unique<
                  mojo::Binding<MojomWilcoDtcSupportdServiceFactory>>(
                  mojo_service_factory, &mojo_service_factory_interface_ptr_);
              DCHECK(mojo_service_factory_interface_ptr_);
              return mojo_service_factory_binding.release();
            }));
  }

  void WriteEcEventToEcEventFile(const EcEvent& ec_event) const {
    ASSERT_EQ(write(ec_event_service_fd_.get(), &ec_event, sizeof(ec_event)),
              sizeof(ec_event));
  }

  dbus::ExportedObject::MethodCallCallback
  bootstrap_mojo_connection_dbus_method() {
    return bootstrap_mojo_connection_dbus_method_;
  }

  const std::string& wilco_dtc_supportd_grpc_uri() const {
    DCHECK(!wilco_dtc_supportd_grpc_uri_.empty());
    return wilco_dtc_supportd_grpc_uri_;
  }

  const std::string& ui_message_receiver_wilco_dtc_grpc_uri() const {
    DCHECK(!ui_message_receiver_wilco_dtc_grpc_uri_.empty());
    return ui_message_receiver_wilco_dtc_grpc_uri_;
  }

  const std::string& wilco_dtc_grpc_uri() const {
    DCHECK(!wilco_dtc_grpc_uri_.empty());
    return wilco_dtc_grpc_uri_;
  }

 private:
  // Perform initialization of the D-Bus object exposed by the tested code.
  void SetUpDBus() {
    const dbus::ObjectPath kDBusObjectPath(kWilcoDtcSupportdServicePath);

    // Expect that the /org/chromium/WilcoDtcSupportd object is exported.
    wilco_dtc_supportd_dbus_object_ = new StrictMock<dbus::MockExportedObject>(
        dbus_bus_.get(), kDBusObjectPath);
    EXPECT_CALL(*dbus_bus_, GetExportedObject(kDBusObjectPath))
        .WillOnce(Return(wilco_dtc_supportd_dbus_object_.get()));

    // Expect that standard methods on the org.freedesktop.DBus.Properties
    // interface are exported.
    EXPECT_CALL(
        *wilco_dtc_supportd_dbus_object_,
        ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesGet, _, _));
    EXPECT_CALL(
        *wilco_dtc_supportd_dbus_object_,
        ExportMethod(dbus::kPropertiesInterface, dbus::kPropertiesSet, _, _));
    EXPECT_CALL(*wilco_dtc_supportd_dbus_object_,
                ExportMethod(dbus::kPropertiesInterface,
                             dbus::kPropertiesGetAll, _, _));

    // Expect that methods on the org.chromium.WilcoDtcSupportdInterface
    // interface are exported.
    EXPECT_CALL(
        *wilco_dtc_supportd_dbus_object_,
        ExportMethod(kWilcoDtcSupportdServiceInterface,
                     kWilcoDtcSupportdBootstrapMojoConnectionMethod, _, _))
        .WillOnce(SaveArg<2 /* method_call_callback */>(
            &bootstrap_mojo_connection_dbus_method_));

    // Run the tested code that exports D-Bus objects and methods.
    scoped_refptr<brillo::dbus_utils::AsyncEventSequencer> dbus_sequencer(
        new brillo::dbus_utils::AsyncEventSequencer());
    core()->RegisterDBusObjectsAsync(dbus_bus_, dbus_sequencer.get());

    // Verify that required D-Bus methods are exported.
    EXPECT_FALSE(bootstrap_mojo_connection_dbus_method_.is_null());
  }

  // Set mock expectations for calls triggered during test destruction.
  void SetDBusShutdownExpectations() {
    EXPECT_CALL(*wilco_dtc_supportd_dbus_object_, Unregister());
  }

  // Creates FIFO to emulates the EC event file used by EC event service.
  void SetUpEcEventService() {
    core()->set_ec_event_service_fd_events_for_testing(POLLIN);
    ASSERT_TRUE(base::CreateDirectory(ec_event_file_path().DirName()));
    ASSERT_EQ(mkfifo(ec_event_file_path().value().c_str(), 0600), 0);
  }

  // Setups |ec_event_service_fd_| FIFO file descriptor. Must be called only
  // after |WilcoDtcSupportdCore::Start()| call. Otherwise, it will block
  // thread.
  void SetUpEcEventServiceFifoWriteEnd() {
    ASSERT_FALSE(ec_event_service_fd_.is_valid());
    ec_event_service_fd_.reset(
        open(ec_event_file_path().value().c_str(), O_WRONLY));
    ASSERT_TRUE(ec_event_service_fd_.is_valid());
  }

  base::FilePath ec_event_file_path() const {
    return temp_dir_.GetPath().Append(kEcEventFilePath);
  }

  base::ScopedTempDir temp_dir_;

  // gRPC URI on which the tested "WilcoDtcSupportd" gRPC service (owned by
  // WilcoDtcSupportdCore) is listening.
  std::string wilco_dtc_supportd_grpc_uri_;
  // gRPC URI on which the fake "WilcoDtc" gRPC service (owned by FakeWilcoDtc)
  // is listening, eligible to receive UI messages.
  std::string ui_message_receiver_wilco_dtc_grpc_uri_;
  // gRPC URI on which the fake "WilcoDtc" gRPC service (owned by FakeWilcoDtc)
  // is listening.
  std::string wilco_dtc_grpc_uri_;

  scoped_refptr<StrictMock<dbus::MockBus>> dbus_bus_ =
      new StrictMock<dbus::MockBus>(dbus::Bus::Options());

  // Mock D-Bus integration helper for the object exposed by the tested code.
  scoped_refptr<StrictMock<dbus::MockExportedObject>>
      wilco_dtc_supportd_dbus_object_;

  // Mojo interface to the service factory exposed by the tested code.
  mojo::InterfacePtr<MojomWilcoDtcSupportdServiceFactory>
      mojo_service_factory_interface_ptr_;

  // Write end of FIFO that emulates EC event file. EC event service
  // operates with read end of FIFO as with usual file.
  // Must be initialized only after |WilcoDtcSupportdCore::Start()| call.
  base::ScopedFD ec_event_service_fd_;

  // Callback that the tested code exposed as the BootstrapMojoConnection D-Bus
  // method.
  dbus::ExportedObject::MethodCallCallback
      bootstrap_mojo_connection_dbus_method_;

  std::unique_ptr<FakeBrowser> fake_browser_;
};

}  // namespace

// Test that the Mojo service gets successfully bootstrapped after the
// BootstrapMojoConnection D-Bus method is called.
TEST_F(StartedWilcoDtcSupportdCoreTest, MojoBootstrapSuccess) {
  FakeMojoFdGenerator fake_mojo_fd_generator;
  SetSuccessMockBindWilcoDtcSupportdMojoService(&fake_mojo_fd_generator);

  EXPECT_TRUE(fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  EXPECT_TRUE(*mojo_service_factory_interface_ptr());
}

// Test failure to bootstrap the Mojo service due to en error returned by
// BindWilcoDtcSupportdMojoService() delegate method.
TEST_F(StartedWilcoDtcSupportdCoreTest, MojoBootstrapErrorToBind) {
  FakeMojoFdGenerator fake_mojo_fd_generator;
  EXPECT_CALL(*core_delegate(),
              BindWilcoDtcSupportdMojoServiceFactoryImpl(_, _))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*core_delegate(), BeginDaemonShutdown());

  EXPECT_FALSE(
      fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  Mock::VerifyAndClearExpectations(core_delegate());
}

// Test that second attempt to bootstrap the Mojo service results in error and
// the daemon shutdown.
TEST_F(StartedWilcoDtcSupportdCoreTest, MojoBootstrapErrorRepeated) {
  FakeMojoFdGenerator first_fake_mojo_fd_generator;
  SetSuccessMockBindWilcoDtcSupportdMojoService(&first_fake_mojo_fd_generator);

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
TEST_F(StartedWilcoDtcSupportdCoreTest, MojoBootstrapSuccessThenAbort) {
  FakeMojoFdGenerator fake_mojo_fd_generator;
  SetSuccessMockBindWilcoDtcSupportdMojoService(&fake_mojo_fd_generator);

  EXPECT_TRUE(fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));

  Mock::VerifyAndClearExpectations(core_delegate());

  EXPECT_CALL(*core_delegate(), BeginDaemonShutdown());

  // Abort the Mojo connection by closing the browser-side endpoint.
  mojo_service_factory_interface_ptr()->reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(core_delegate());
}

namespace {

// Tests for the WilcoDtcSupportdCore class with the already established Mojo
// connection to the fake browser and gRPC communication with the fake
// wilco_dtc.
class BootstrappedWilcoDtcSupportdCoreTest
    : public StartedWilcoDtcSupportdCoreTest {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(StartedWilcoDtcSupportdCoreTest::SetUp());

    FakeMojoFdGenerator fake_mojo_fd_generator;
    SetSuccessMockBindWilcoDtcSupportdMojoService(&fake_mojo_fd_generator);
    ASSERT_TRUE(
        fake_browser()->BootstrapMojoConnection(&fake_mojo_fd_generator));
    ASSERT_TRUE(*mojo_service_factory_interface_ptr());

    fake_wilco_dtc_ = std::make_unique<FakeWilcoDtc>(
        wilco_dtc_grpc_uri(), wilco_dtc_supportd_grpc_uri());

    fake_ui_message_receiver_wilco_dtc_ =
        std::make_unique<FakeWilcoDtc>(ui_message_receiver_wilco_dtc_grpc_uri(),
                                       wilco_dtc_supportd_grpc_uri());
  }

  void TearDown() override {
    fake_wilco_dtc_.reset();
    fake_ui_message_receiver_wilco_dtc_.reset();
    StartedWilcoDtcSupportdCoreTest::TearDown();
  }

  FakeWilcoDtc* fake_ui_message_receiver_wilco_dtc() {
    return fake_ui_message_receiver_wilco_dtc_.get();
  }

  FakeWilcoDtc* fake_wilco_dtc() { return fake_wilco_dtc_.get(); }

  base::Callback<void(mojo::ScopedHandle)> fake_browser_valid_handle_callback(
      const base::Closure& callback,
      const std::string& expected_response_json_message) {
    return base::Bind(
        [](const base::Closure& callback,
           const std::string& expected_response_json_message,
           mojo::ScopedHandle response_json_message_handle) {
          std::unique_ptr<base::SharedMemory> shared_memory =
              GetReadOnlySharedMemoryFromMojoHandle(
                  std::move(response_json_message_handle));
          ASSERT_TRUE(shared_memory);
          ASSERT_EQ(
              expected_response_json_message,
              std::string(static_cast<const char*>(shared_memory->memory()),
                          shared_memory->mapped_size()));
          callback.Run();
        },
        callback, expected_response_json_message);
  }

  base::Callback<void(mojo::ScopedHandle)> fake_browser_invalid_handle_callback(
      const base::Closure& callback) {
    return base::Bind(
        [](const base::Closure& callback,
           mojo::ScopedHandle response_json_message_handle) {
          ASSERT_FALSE(response_json_message_handle.is_valid());
          callback.Run();
        },
        callback);
  }

  MockMojomWilcoDtcSupportdClient* wilco_dtc_supportd_client() {
    return fake_browser()->wilco_dtc_supportd_client();
  }

 private:
  std::unique_ptr<FakeWilcoDtc> fake_ui_message_receiver_wilco_dtc_;
  std::unique_ptr<FakeWilcoDtc> fake_wilco_dtc_;
};

}  // namespace

// Test that the UI message receiver wilco_dtc will receive message from
// browser.
// TODO(crbug.com/946330): Disabled due to flakiness.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest,
       DISABLED_SendGrpcUiMessageToWilcoDtc) {
  const std::string json_message = "{\"some_key\": \"some_value\"}";
  const std::string response_json_message = "{\"key\": \"value\"}";

  base::RunLoop run_loop_wilco_dtc;
  base::RunLoop run_loop_fake_browser;

  fake_ui_message_receiver_wilco_dtc()->set_handle_message_from_ui_callback(
      run_loop_wilco_dtc.QuitClosure());
  fake_ui_message_receiver_wilco_dtc()
      ->set_handle_message_from_ui_json_message_response(response_json_message);
  fake_wilco_dtc()->set_handle_message_from_ui_callback(base::Bind([]() {
    // The wilco_dtc not eligible to receive messages from UI must not
    // receive them.
    FAIL();
  }));

  auto callback = fake_browser_valid_handle_callback(
      run_loop_fake_browser.QuitClosure(), response_json_message);
  EXPECT_TRUE(fake_browser()->SendUiMessageToWilcoDtc(json_message, callback));

  run_loop_wilco_dtc.Run();
  run_loop_fake_browser.Run();
  EXPECT_EQ(json_message, fake_ui_message_receiver_wilco_dtc()
                              ->handle_message_from_ui_actual_json_message());
}

// Test that the UI message receiver wilco_dtc will not receive message from
// browser if JSON message is invalid.
// TODO(crbug.com/946330): Disabled due to flakiness.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest,
       DISABLED_SendGrpcUiMessageToWilcoDtcInvalidJSON) {
  const std::string json_message = "{'some_key': 'some_value'}";

  base::RunLoop run_loop_fake_browser;

  auto callback =
      fake_browser_invalid_handle_callback(run_loop_fake_browser.QuitClosure());
  EXPECT_TRUE(fake_browser()->SendUiMessageToWilcoDtc(json_message, callback));

  run_loop_fake_browser.Run();
  // There's no reliable way to wait till the wrong HandleMessageFromUi(), if
  // the tested code is buggy and calls it, gets executed. The RunUntilIdle() is
  // used to make the test failing at least with some probability in case of
  // such a bug.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_ui_message_receiver_wilco_dtc()
                   ->handle_message_from_ui_actual_json_message()
                   .has_value());
}

// Test that the UI message receiver wilco_dtc will receive message from
// browser.
// TODO(crbug.com/946330): Disabled due to flakiness.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest,
       DISABLED_SendGrpcUiMessageToWilcoDtcInvalidResponseJSON) {
  const std::string json_message = "{\"some_key\": \"some_value\"}";
  const std::string response_json_message = "{'key': 'value'}";

  base::RunLoop run_loop_wilco_dtc;
  base::RunLoop run_loop_fake_browser;

  fake_ui_message_receiver_wilco_dtc()->set_handle_message_from_ui_callback(
      run_loop_wilco_dtc.QuitClosure());
  fake_ui_message_receiver_wilco_dtc()
      ->set_handle_message_from_ui_json_message_response(response_json_message);

  auto callback =
      fake_browser_invalid_handle_callback(run_loop_fake_browser.QuitClosure());
  EXPECT_TRUE(fake_browser()->SendUiMessageToWilcoDtc(json_message, callback));

  run_loop_wilco_dtc.Run();
  run_loop_fake_browser.Run();
  EXPECT_EQ(json_message, fake_ui_message_receiver_wilco_dtc()
                              ->handle_message_from_ui_actual_json_message());
}

// Test that wilco_dtc will be notified about configuration changes from
// browser.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest, NotifyConfigurationDataChanged) {
  base::RunLoop run_loop;
  const base::Closure barrier_closure =
      BarrierClosure(2, run_loop.QuitClosure());

  fake_ui_message_receiver_wilco_dtc()->set_configuration_data_changed_callback(
      barrier_closure);
  fake_wilco_dtc()->set_configuration_data_changed_callback(barrier_closure);

  fake_browser()->NotifyConfigurationDataChanged();
  run_loop.Run();
}

// Test that the GetProcData() method exposed by the daemon's gRPC server
// returns a dump of the corresponding file from the disk.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest, GetProcDataGrpcCall) {
  const std::string kFakeFileContents = "foo";
  const base::FilePath file_path = temp_dir_path().Append("proc/uptime");
  ASSERT_TRUE(WriteFileAndCreateParentDirs(file_path, kFakeFileContents));

  grpc_api::GetProcDataRequest request;
  request.set_type(grpc_api::GetProcDataRequest::FILE_UPTIME);
  std::unique_ptr<grpc_api::GetProcDataResponse> response;
  base::RunLoop run_loop;
  fake_wilco_dtc()->GetProcData(request,
                                MakeAsyncResponseWriter(&response, &run_loop));
  run_loop.Run();

  ASSERT_TRUE(response);
  grpc_api::GetProcDataResponse expected_response;
  expected_response.add_file_dump();
  expected_response.mutable_file_dump(0)->set_path(file_path.value());
  expected_response.mutable_file_dump(0)->set_canonical_path(file_path.value());
  expected_response.mutable_file_dump(0)->set_contents(kFakeFileContents);
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      *response, expected_response))
      << "Obtained: " << response->ShortDebugString()
      << ",\nExpected: " << expected_response.ShortDebugString();
}

// Test that the GetEcTelemetry() method exposed by the daemon's gRPC server
// writes payload to devfs file exposed by the EC driver and reads response
// using the same file.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest, GetEcTelemetryGrpcCall) {
  const base::FilePath file_path =
      temp_dir_path().Append(kEcGetTelemetryFilePath);
  const std::string kRequestPayload = "12345";
  const std::string kResponsePayload = "67890";

  // Write request and response payload because EC telemetry char device is
  // non-seekable.
  ASSERT_TRUE(WriteFileAndCreateParentDirs(file_path,
                                           kRequestPayload + kResponsePayload));

  grpc_api::GetEcTelemetryRequest request;
  request.set_payload(kRequestPayload);
  std::unique_ptr<grpc_api::GetEcTelemetryResponse> response;
  base::RunLoop run_loop;
  fake_wilco_dtc()->GetEcTelemetry(
      request, MakeAsyncResponseWriter(&response, &run_loop));
  run_loop.Run();

  ASSERT_TRUE(response);
  grpc_api::GetEcTelemetryResponse expected_response;
  expected_response.set_status(grpc_api::GetEcTelemetryResponse::STATUS_OK);
  expected_response.set_payload(kResponsePayload);
  EXPECT_THAT(*response, ProtobufEquals(expected_response))
      << "Actual: {" << response->ShortDebugString() << "}";
}

// Test that the GetEcProperty() method exposed by the daemon's gRPC server
// returns a dump of the corresponding file from the disk.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest, GetEcPropertyGrpcCall) {
  const base::FilePath file_path = temp_dir_path()
                                       .Append(kEcDriverSysfsPath)
                                       .Append(kEcDriverSysfsPropertiesPath)
                                       .Append(kEcPropertyGlobalMicMuteLed);
  const std::string kFakeFileContents = "1";
  ASSERT_TRUE(WriteFileAndCreateParentDirs(file_path, kFakeFileContents));

  grpc_api::GetEcPropertyRequest request;
  request.set_property(
      grpc_api::GetEcPropertyRequest::PROPERTY_GLOBAL_MIC_MUTE_LED);
  std::unique_ptr<grpc_api::GetEcPropertyResponse> response;
  base::RunLoop run_loop;
  fake_wilco_dtc()->GetEcProperty(
      request, MakeAsyncResponseWriter(&response, &run_loop));
  run_loop.Run();

  ASSERT_TRUE(response);
  grpc_api::GetEcPropertyResponse expected_response;
  expected_response.set_status(grpc_api::GetEcPropertyResponse::STATUS_OK);
  expected_response.set_payload(kFakeFileContents);
  EXPECT_THAT(*response, ProtobufEquals(expected_response))
      << "Actual: {" << response->ShortDebugString() << "}";
}

// Test that PerformWebRequest() method exposed by the daemon's gRPC returns a
// Web request response from the browser.
// TODO(crbug.com/946330): Disabled due to flakiness.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest,
       DISABLED_PerformWebRequestToBrowser) {
  constexpr char kHttpsUrl[] = "https://www.google.com";
  constexpr int kHttpStatusOk = 200;

  grpc_api::PerformWebRequestParameter request;
  request.set_http_method(
      grpc_api::PerformWebRequestParameter::HTTP_METHOD_GET);
  request.set_url(kHttpsUrl);

  std::unique_ptr<grpc_api::PerformWebRequestResponse> response;
  {
    base::RunLoop run_loop;
    fake_wilco_dtc()->PerformWebRequest(
        request, MakeAsyncResponseWriter(&response, &run_loop));
    run_loop.Run();
  }

  ASSERT_TRUE(response);
  grpc_api::PerformWebRequestResponse expected_response;
  expected_response.set_status(grpc_api::PerformWebRequestResponse::STATUS_OK);
  expected_response.set_http_status(kHttpStatusOk);
  EXPECT_THAT(*response, ProtobufEquals(expected_response))
      << "Actual: {" << response->ShortDebugString() << "}";
}

// Test that GetConfigurationData() method exposed by the daemon's gRPC returns
// a response from the browser.
TEST_F(BootstrappedWilcoDtcSupportdCoreTest, GetConfigurationDataFromBrowser) {
  constexpr char kFakeJsonConfigurationData[] =
      "{\"fake-message\": \"Fake JSON configuration data\"}";
  EXPECT_CALL(*wilco_dtc_supportd_client(), GetConfigurationData(_))
      .WillOnce(
          Invoke([kFakeJsonConfigurationData](
                     const base::Callback<void(const std::string&)>& callback) {
            callback.Run(kFakeJsonConfigurationData);
          }));
  std::unique_ptr<grpc_api::GetConfigurationDataResponse> response;
  {
    base::RunLoop run_loop;
    grpc_api::GetConfigurationDataRequest request;
    fake_wilco_dtc()->GetConfigurationData(
        request, MakeAsyncResponseWriter(&response, &run_loop));
    run_loop.Run();
  }

  ASSERT_TRUE(response);
  grpc_api::GetConfigurationDataResponse expected_response;
  expected_response.set_json_configuration_data(kFakeJsonConfigurationData);
  EXPECT_THAT(*response, ProtobufEquals(expected_response))
      << "Actual: {" << response->ShortDebugString() << "}";
}

namespace {

// Fake types to be used to emulate EC events.
const uint16_t kFakeEcEventType1 = 0xabcd;
const uint16_t kFakeEcEventType2 = 0x1234;

// Tests for EC event service.
class EcEventServiceBootstrappedWilcoDtcSupportdCoreTest
    : public BootstrappedWilcoDtcSupportdCoreTest {
 protected:
  // Holds EC event type and payload of |grpc_api::HandleEcNotificationResponse|
  using GrpcEvent = std::pair<uint16_t, std::string>;

  std::string GetPayload(size_t expected_size_in_bytes) const {
    return std::string(reinterpret_cast<const char*>(kPayload),
                       expected_size_in_bytes);
  }

  void EmulateEcEvent(uint16_t size_in_words, uint16_t type) const {
    WriteEcEventToEcEventFile(GetEcEvent(size_in_words, type));
  }

  void ExpectAllFakeWilcoDtcReceivedEcEvents(
      const std::multiset<GrpcEvent>& expected_ec_events) {
    base::RunLoop run_loop;
    auto barrier_closure =
        BarrierClosure(2 * expected_ec_events.size(), run_loop.QuitClosure());

    std::multiset<GrpcEvent> fake_wilco_dtc_ec_events;
    std::multiset<GrpcEvent> fake_ui_message_receiver_wilco_dtc_ec_events;
    SetupFakeWilcoDtcEcEventCallback(barrier_closure, fake_wilco_dtc(),
                                     &fake_wilco_dtc_ec_events);
    SetupFakeWilcoDtcEcEventCallback(
        barrier_closure, fake_ui_message_receiver_wilco_dtc(),
        &fake_ui_message_receiver_wilco_dtc_ec_events);

    run_loop.Run();

    EXPECT_EQ(fake_wilco_dtc_ec_events, expected_ec_events);
    EXPECT_EQ(fake_ui_message_receiver_wilco_dtc_ec_events, expected_ec_events);
  }

 private:
  const uint16_t kData[6]{0x0102, 0x1314, 0x2526, 0x3738, 0x494a, 0x5b5c};
  // |kData| bytes little endian representation.
  const uint8_t kPayload[12]{0x02, 0x01, 0x14, 0x13, 0x26, 0x25,
                             0x38, 0x37, 0x4a, 0x49, 0x5c, 0x5b};

  EcEvent GetEcEvent(uint16_t size_in_words, uint16_t type) const {
    return EcEvent(size_in_words, static_cast<EcEvent::Type>(type), kData);
  }

  void SetupFakeWilcoDtcEcEventCallback(const base::Closure& callback,
                                        FakeWilcoDtc* fake_wilco_dtc,
                                        std::multiset<GrpcEvent>* events_out) {
    DCHECK(fake_wilco_dtc);
    DCHECK(events_out);
    fake_wilco_dtc->set_handle_ec_event_request_callback(base::BindRepeating(
        [](const base::Closure& callback, std::multiset<GrpcEvent>* events_out,
           int32_t type, const std::string& payload) {
          DCHECK(events_out);
          events_out->insert({type, payload});
          callback.Run();
        },
        callback, events_out));
  }
};

}  // namespace

// Test that the method |HandleEcNotification()| exposed by wilco_dtc gRPC is
// called by wilco_dtc support daemon.
TEST_F(EcEventServiceBootstrappedWilcoDtcSupportdCoreTest,
       SendGrpcEventToWilcoDtcSize0) {
  EmulateEcEvent(0, kFakeEcEventType1);
  EXPECT_CALL(*wilco_dtc_supportd_client(), HandleEvent(_));
  ExpectAllFakeWilcoDtcReceivedEcEvents({{kFakeEcEventType1, GetPayload(0)}});
}

// Test that the method |HandleEcNotification()| exposed by wilco_dtc gRPC is
// called by wilco_dtc support daemon.
TEST_F(EcEventServiceBootstrappedWilcoDtcSupportdCoreTest,
       SendGrpcEventToWilcoDtcSize5) {
  EmulateEcEvent(5, kFakeEcEventType1);
  EXPECT_CALL(*wilco_dtc_supportd_client(), HandleEvent(_));
  ExpectAllFakeWilcoDtcReceivedEcEvents({{kFakeEcEventType1, GetPayload(10)}});
}

// Test that the method |HandleEcNotification()| exposed by wilco_dtc gRPC is
// called by wilco_dtc support daemon.
TEST_F(EcEventServiceBootstrappedWilcoDtcSupportdCoreTest,
       SendGrpcEventToWilcoDtcSize6) {
  EmulateEcEvent(6, kFakeEcEventType1);
  EXPECT_CALL(*wilco_dtc_supportd_client(), HandleEvent(_));
  ExpectAllFakeWilcoDtcReceivedEcEvents({{kFakeEcEventType1, GetPayload(12)}});
}

// Test that the method |HandleEcNotification()| exposed by wilco_dtc gRPC is
// called by wilco_dtc support daemon multiple times.
TEST_F(EcEventServiceBootstrappedWilcoDtcSupportdCoreTest,
       SendGrpcEventToWilcoDtcMultipleEvents) {
  EmulateEcEvent(3, kFakeEcEventType1);
  EmulateEcEvent(4, kFakeEcEventType2);
  EXPECT_CALL(*wilco_dtc_supportd_client(), HandleEvent(_)).Times(2);
  ExpectAllFakeWilcoDtcReceivedEcEvents(
      {{kFakeEcEventType1, GetPayload(6)}, {kFakeEcEventType2, GetPayload(8)}});
}

// Test that the method |HandleEcNotification()| exposed by wilco_dtc gRPC is
// not called by wilco_dtc support daemon when |ec_event.size| exceeds
// allocated data array.
TEST_F(EcEventServiceBootstrappedWilcoDtcSupportdCoreTest,
       SendGrpcEventToWilcoDtcInvalidSize) {
  EmulateEcEvent(3, kFakeEcEventType1);
  EmulateEcEvent(7, kFakeEcEventType2);
  EXPECT_CALL(*wilco_dtc_supportd_client(), HandleEvent(_)).Times(2);
  // Expect only EC event with valid size.
  ExpectAllFakeWilcoDtcReceivedEcEvents({{kFakeEcEventType1, GetPayload(6)}});
}

}  // namespace diagnostics
