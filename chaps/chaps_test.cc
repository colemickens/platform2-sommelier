// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chaps client unit tests. These tests exercise the client layer (chaps.cc) and
// use a mock for the proxy interface so no D-Bus code is run.
//

#include "chaps/chaps_proxy_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "pkcs11/cryptoki.h"

using ::testing::_;
using ::testing::Return;

// Initialize / Finalize tests
TEST(TestInitialize,InitializeNULL) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_CALL(proxy, Connect("default"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proxy, Disconnect());
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,InitializeFailConnect) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_CALL(proxy, Connect("default"))
      .WillRepeatedly(Return(false));
  EXPECT_EQ(CKR_GENERAL_ERROR, C_Initialize(NULL_PTR));
}

TEST(TestInitialize,InitializeOutOfMem) {
  chaps::EnableMockProxy(NULL, false);
  EXPECT_EQ(CKR_HOST_MEMORY, C_Initialize(NULL_PTR));
  chaps::DisableMockProxy();
}

TEST(TestInitialize,InitializeTwice) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_CALL(proxy, Connect("default"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proxy, Disconnect());
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_CRYPTOKI_ALREADY_INITIALIZED, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,InitializeWithArgs) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_CALL(proxy, Connect("default"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proxy, Disconnect());
  CK_C_INITIALIZE_ARGS args;
  memset(&args, 0, sizeof(args));
  EXPECT_EQ(CKR_OK, C_Initialize(&args));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,FinalizeWithArgs) {
  EXPECT_EQ(CKR_ARGUMENTS_BAD, C_Finalize((void*)1));
}

TEST(TestInitialize,FinalizeNotInit) {
  EXPECT_EQ(CKR_CRYPTOKI_NOT_INITIALIZED, C_Finalize(NULL_PTR));
}

TEST(TestInitialize,Reinitialize) {
  chaps::ChapsProxyMock proxy(false);
  EXPECT_CALL(proxy, Connect("default"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(proxy, Disconnect());
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Finalize(NULL_PTR));
  EXPECT_EQ(CKR_OK, C_Initialize(NULL_PTR));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

