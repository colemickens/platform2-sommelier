// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tpm_manager/server/mock_local_data_store.h"
#include "tpm_manager/server/mock_tpm_initializer.h"
#include "tpm_manager/server/mock_tpm_status.h"
#include "tpm_manager/server/tpm_manager_service.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace tpm_manager {

// A test fixture that takes care of message loop management and configuring a
// TpmManagerService instance with mock dependencies.
class TpmManagerServiceTest : public testing::Test {
 public:
  ~TpmManagerServiceTest() override = default;
  void SetUp() override {
    service_.reset(new TpmManagerService(true /*wait_for_ownership*/,
                                         &mock_local_data_store_,
                                         &mock_tpm_status_,
                                         &mock_tpm_initializer_));
    SetupService();
  }

 protected:
  void Run() {
    run_loop_.Run();
  }

  void RunServiceWorkerAndQuit() {
    // Run out the service worker loop by posting a new command and waiting for
    // the response.
    auto callback = [this](const GetTpmStatusReply& reply) {
      Quit();
    };
    GetTpmStatusRequest request;
    service_->GetTpmStatus(request, base::Bind(callback));
    Run();
  }

  void Quit() {
    run_loop_.Quit();
  }

  void SetupService() {
    CHECK(service_->Initialize());
  }

  NiceMock<MockLocalDataStore> mock_local_data_store_;
  NiceMock<MockTpmInitializer> mock_tpm_initializer_;
  NiceMock<MockTpmStatus> mock_tpm_status_;
  std::unique_ptr<TpmManagerService> service_;

 private:
  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
};

// Tests must call SetupService().
class TpmManagerServiceTest_NoWaitForOwnership : public TpmManagerServiceTest {
 public:
  ~TpmManagerServiceTest_NoWaitForOwnership() override = default;
  void SetUp() override {
    service_.reset(new TpmManagerService(false /*wait_for_ownership*/,
                                         &mock_local_data_store_,
                                         &mock_tpm_status_,
                                         &mock_tpm_initializer_));
  }
};

TEST_F(TpmManagerServiceTest_NoWaitForOwnership, AutoInitialize) {
  // Make sure InitializeTpm doesn't get multiple calls.
  EXPECT_CALL(mock_tpm_initializer_, InitializeTpm()).Times(1);
  SetupService();
  RunServiceWorkerAndQuit();
}

TEST_F(TpmManagerServiceTest_NoWaitForOwnership, AutoInitializeNoTpm) {
  EXPECT_CALL(mock_tpm_status_, IsTpmEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_initializer_, InitializeTpm()).Times(0);
  SetupService();
  RunServiceWorkerAndQuit();
}

TEST_F(TpmManagerServiceTest_NoWaitForOwnership, AutoInitializeFailure) {
  EXPECT_CALL(mock_tpm_initializer_, InitializeTpm())
      .WillRepeatedly(Return(false));
  SetupService();
  RunServiceWorkerAndQuit();
}

TEST_F(TpmManagerServiceTest, NoAutoInitialize) {
  EXPECT_CALL(mock_tpm_initializer_, InitializeTpm()).Times(0);
  RunServiceWorkerAndQuit();
}

TEST_F(TpmManagerServiceTest, GetTpmStatusSuccess) {
  EXPECT_CALL(mock_tpm_status_, GetDictionaryAttackInfo(_, _, _, _))
      .WillRepeatedly(Invoke([](int* counter, int* threshold, bool* lockout,
                                int* seconds_remaining) {
        *counter = 5;
        *threshold = 6;
        *lockout = true;
        *seconds_remaining = 7;
        return true;
      }));
  LocalData local_data;
  local_data.set_owner_password("test");
  EXPECT_CALL(mock_local_data_store_, Read(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(local_data), Return(true)));

  auto callback = [this](const GetTpmStatusReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.enabled());
    EXPECT_TRUE(reply.owned());
    EXPECT_EQ("test", reply.local_data().owner_password());
    EXPECT_EQ(5, reply.dictionary_attack_counter());
    EXPECT_EQ(6, reply.dictionary_attack_threshold());
    EXPECT_TRUE(reply.dictionary_attack_lockout_in_effect());
    EXPECT_EQ(7, reply.dictionary_attack_lockout_seconds_remaining());
    Quit();
  };
  GetTpmStatusRequest request;
  service_->GetTpmStatus(request, base::Bind(callback));
  Run();
}

TEST_F(TpmManagerServiceTest, GetTpmStatusLocalDataFailure) {
  EXPECT_CALL(mock_local_data_store_, Read(_))
      .WillRepeatedly(Return(false));
  auto callback = [this](const GetTpmStatusReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.enabled());
    EXPECT_TRUE(reply.owned());
    EXPECT_FALSE(reply.has_local_data());
    EXPECT_TRUE(reply.has_dictionary_attack_counter());
    EXPECT_TRUE(reply.has_dictionary_attack_threshold());
    EXPECT_TRUE(reply.has_dictionary_attack_lockout_in_effect());
    EXPECT_TRUE(reply.has_dictionary_attack_lockout_seconds_remaining());
    Quit();
  };
  GetTpmStatusRequest request;
  service_->GetTpmStatus(request, base::Bind(callback));
  Run();
}

TEST_F(TpmManagerServiceTest, GetTpmStatusNoTpm) {
  EXPECT_CALL(mock_tpm_status_, IsTpmEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_status_, GetDictionaryAttackInfo(_, _, _, _))
      .WillRepeatedly(Return(false));
  auto callback = [this](const GetTpmStatusReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.enabled());
    EXPECT_TRUE(reply.owned());
    EXPECT_TRUE(reply.has_local_data());
    EXPECT_FALSE(reply.has_dictionary_attack_counter());
    EXPECT_FALSE(reply.has_dictionary_attack_threshold());
    EXPECT_FALSE(reply.has_dictionary_attack_lockout_in_effect());
    EXPECT_FALSE(reply.has_dictionary_attack_lockout_seconds_remaining());
    Quit();
  };
  GetTpmStatusRequest request;
  service_->GetTpmStatus(request, base::Bind(callback));
  Run();
}

TEST_F(TpmManagerServiceTest, TakeOwnershipSuccess) {
  // Make sure InitializeTpm doesn't get multiple calls.
  EXPECT_CALL(mock_tpm_initializer_, InitializeTpm()).Times(1);
  auto callback = [this](const TakeOwnershipReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    Quit();
  };
  TakeOwnershipRequest request;
  service_->TakeOwnership(request, base::Bind(callback));
  Run();
}

TEST_F(TpmManagerServiceTest, TakeOwnershipFailure) {
  EXPECT_CALL(mock_tpm_initializer_, InitializeTpm())
      .WillRepeatedly(Return(false));
  auto callback = [this](const TakeOwnershipReply& reply) {
    EXPECT_EQ(STATUS_UNEXPECTED_DEVICE_ERROR, reply.status());
    Quit();
  };
  TakeOwnershipRequest request;
  service_->TakeOwnership(request, base::Bind(callback));
  Run();
}

TEST_F(TpmManagerServiceTest, TakeOwnershipNoTpm) {
  EXPECT_CALL(mock_tpm_status_, IsTpmEnabled()).WillRepeatedly(Return(false));
  auto callback = [this](const TakeOwnershipReply& reply) {
    EXPECT_EQ(STATUS_NOT_AVAILABLE, reply.status());
    Quit();
  };
  TakeOwnershipRequest request;
  service_->TakeOwnership(request, base::Bind(callback));
  Run();
}

TEST_F(TpmManagerServiceTest_NoWaitForOwnership,
       TakeOwnershipAfterAutoInitialize) {
  EXPECT_CALL(mock_tpm_initializer_, InitializeTpm()).Times(AtLeast(2));
  SetupService();
  auto callback = [this](const TakeOwnershipReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    Quit();
  };
  TakeOwnershipRequest request;
  service_->TakeOwnership(request, base::Bind(callback));
  Run();
}

}  // namespace tpm_manager
