// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/libvda/libvda.h"

#include <memory>

#include <base/logging.h>
#include <base/macros.h>
#include <gtest/gtest.h>

namespace {

struct ImplDeleter {
  void operator()(void* impl) { deinitialize(impl); }
};

using ImplPtr = std::unique_ptr<void, ImplDeleter>;

struct SessionDeleter {
  explicit SessionDeleter(void* impl = nullptr) : impl_(impl) {}

  void operator()(vda_session_info_t* session) {
    close_decode_session(impl_, session);
  }

 private:
  void* impl_;
};

using SessionPtr = std::unique_ptr<vda_session_info_t, SessionDeleter>;

ImplPtr SetupImpl(vda_impl_type_t impl_type) {
  return ImplPtr(initialize(impl_type));
}

SessionPtr SetupSession(const ImplPtr& impl, vda_profile_t profile) {
  return SessionPtr(init_decode_session(impl.get(), profile),
                    SessionDeleter(impl.get()));
}

}  // namespace

class LibvdaTest : public ::testing::Test {
 public:
  LibvdaTest() = default;
  ~LibvdaTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(LibvdaTest);
};

// Test that the fake implementation initializes and deinitializes
// successfully.
TEST_F(LibvdaTest, InitializeFake) {
  ImplPtr impl = SetupImpl(FAKE);
  EXPECT_NE(impl, nullptr);
}

// Test that the fake implementation creates and closes a decode session
// successfully.
TEST_F(LibvdaTest, InitDecodeSessionFake) {
  ImplPtr impl = SetupImpl(FAKE);
  SessionPtr session = SetupSession(impl, H264PROFILE_MAIN);
  ASSERT_NE(session, nullptr);
  EXPECT_NE(session->ctx, nullptr);
  EXPECT_GT(session->event_pipe_fd, 0);
}

// Test that the fake implementation processes a decode event, and echoes back
// a PICTURE_READY event that can be read from the event FD.
TEST_F(LibvdaTest, DecodeAndGetPictureReadyEventFake) {
  ImplPtr impl = SetupImpl(FAKE);
  SessionPtr session = SetupSession(impl, H264PROFILE_MAIN);
  int32_t fake_bitstream_id = 12345;
  vda_decode(session->ctx, fake_bitstream_id /* bitstream_id */, 1 /* fd */,
             0 /* offset */, 0 /* bytes_used */);
  vda_event_t event;
  ASSERT_GT(read(session->event_pipe_fd, &event, sizeof(vda_event_t)), 0);
  EXPECT_EQ(event.event_type, PICTURE_READY);
  EXPECT_EQ(event.event_data.picture_ready.bitstream_id, fake_bitstream_id);
}

// Test that the GPU implementation initializes and deinitializes successfully.
TEST_F(LibvdaTest, InitializeGpu) {
  ImplPtr impl = SetupImpl(GAVDA);
  EXPECT_NE(impl, nullptr);
}

// Test that the GPU implementation creates and closes a decode session
// successfully.
TEST_F(LibvdaTest, InitDecodeSessionGpu) {
  ImplPtr impl = SetupImpl(GAVDA);
  SessionPtr session = SetupSession(impl, H264PROFILE_MAIN);
  ASSERT_NE(session, nullptr);
  EXPECT_NE(session->ctx, nullptr);
  EXPECT_GT(session->event_pipe_fd, 0);
}
