// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <brillo/errors/error.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/common/mojo_test_utils.h"
#include "diagnostics/wilco_dtc_supportd/dbus_service.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArg;

namespace diagnostics {

namespace {

class MockDBusServiceDelegate : public DBusService::Delegate {
 public:
  // Delegate overrides:
  bool StartMojoServiceFactory(base::ScopedFD mojo_pipe_fd,
                               std::string* error_message) override {
    // Redirect to a separate mockable method to workaround GMock's issues with
    // move-only parameters.
    return StartMojoServiceFactoryImpl(mojo_pipe_fd.get(), error_message);
  }

  MOCK_METHOD(bool, StartMojoServiceFactoryImpl, (int, std::string*));
};

// Tests for the DBusService class.
class DBusServiceTest : public testing::Test {
 protected:
  StrictMock<MockDBusServiceDelegate> delegate_;
  DBusService service_{&delegate_};
};

// Test that BootstrapMojoConnection() successfully calls into the delegate
// method when called with a valid file descriptor.
TEST_F(DBusServiceTest, BootstrapMojoConnectionBasic) {
  const FakeMojoFdGenerator fake_mojo_fd_generator;

  EXPECT_CALL(delegate_, StartMojoServiceFactoryImpl(_, _))
      .WillOnce(DoAll(
          WithArg<0 /* mojo_pipe_fd */>(
              Invoke([&fake_mojo_fd_generator](int mojo_pipe_fd) {
                EXPECT_TRUE(fake_mojo_fd_generator.IsDuplicateFd(mojo_pipe_fd));
              })),
          Return(true)));

  brillo::ErrorPtr error;
  EXPECT_TRUE(service_.BootstrapMojoConnection(
      &error, fake_mojo_fd_generator.MakeFd()));
  EXPECT_FALSE(error);
}

// Test that BootstrapMojoConnection() fails when an empty file descriptor is
// supplied.
TEST_F(DBusServiceTest, BootstrapMojoConnectionInvalidFd) {
  brillo::ErrorPtr error;
  EXPECT_FALSE(service_.BootstrapMojoConnection(
      &error, base::ScopedFD() /* mojo_pipe_fd */));
  EXPECT_TRUE(error);
}

}  // namespace

}  // namespace diagnostics
