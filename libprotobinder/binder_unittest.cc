// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_manager.h"

#include <string.h>

#include <base/bind.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest.h>

#include "libprotobinder/binder_driver_stub.h"
#include "libprotobinder/binder_host.h"
#include "libprotobinder/binder_proxy.h"
#include "libprotobinder/protobinder.h"

namespace protobinder {

class BinderTest : public testing::Test {
 public:
  BinderTest()
      : got_death_callback_(false),
        got_transact_callback_(false),
        weak_ptr_factory_(this) {
    // Configure BinderManager to use the stub driver.
    // BinderManager takes ownership of BinderDriverStub.
    driver_ = new BinderDriverStub();
    scoped_ptr<BinderManager> manager(
        new BinderManager(std::unique_ptr<BinderDriverInterface>(driver_)));

    BinderManagerInterface::SetForTesting(manager.Pass());
  }
  ~BinderTest() override {
    BinderManagerInterface::SetForTesting(scoped_ptr<BinderManagerInterface>());
  }

 protected:
  // Checks binder transaction data presented to the driver
  // matches what is provided in a transaction.
  void CheckTransaction(struct binder_transaction_data* data,
                        uint32_t handle,
                        uint32_t code,
                        bool one_way) {
    ASSERT_NE(nullptr, data);
    EXPECT_EQ(handle, data->target.handle);
    EXPECT_EQ(code, data->code);
    EXPECT_EQ(code, data->code);

    unsigned int flags = TF_ACCEPT_FDS;
    flags |= one_way ? TF_ONE_WAY : 0;
    EXPECT_EQ(flags, data->flags);
  }

  void HandleBinderDeath() { got_death_callback_ = true; }
  void HandleTransaction() { got_transact_callback_ = true; }

  BinderDriverStub* driver_;  // Not owned.
  bool got_death_callback_;
  bool got_transact_callback_;
  base::WeakPtrFactory<BinderTest> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BinderTest);
};

class HostTest : public BinderHost {
 public:
  explicit HostTest(const base::Closure& closure)
      : transact_callback_(closure) {}
  ~HostTest() override {}
  int OnTransact(uint32_t code,
                 Parcel* data,
                 Parcel* reply,
                 bool one_way) override {
    EXPECT_EQ(10, code);
    int val = -1;
    EXPECT_TRUE(data->ReadInt32(&val));
    EXPECT_EQ(0xDEAD, val);
    reply->WriteInt32(0xC0DE);
    transact_callback_.Run();
    return SUCCESS;
  }

 private:
  base::Closure transact_callback_;
  DISALLOW_COPY_AND_ASSIGN(HostTest);
};

TEST_F(BinderTest, BasicTransaction) {
  Parcel data;
  uint32_t handle = 0x1111;
  uint32_t code = 0x2222;
  bool one_way = true;
  int ret;

  ret = BinderManagerInterface::Get()->Transact(handle, code, data, nullptr,
                                                one_way);
  EXPECT_EQ(SUCCESS, ret);
  CheckTransaction(driver_->LastTransactionData(), handle, code, one_way);

  handle = 0x3333;
  code = 0x4444;
  ret = BinderManagerInterface::Get()->Transact(handle, code, data, nullptr,
                                                one_way);
  EXPECT_EQ(SUCCESS, ret);
  CheckTransaction(driver_->LastTransactionData(), handle, code, one_way);

  handle = 0x5555;
  code = 0x6666;
  ret = BinderManagerInterface::Get()->Transact(handle, code, data, nullptr,
                                                one_way);
  EXPECT_EQ(SUCCESS, ret);
  CheckTransaction(driver_->LastTransactionData(), handle, code, one_way);

  EXPECT_EQ(0, driver_->LastTransactionData()->data_size);
  EXPECT_EQ(0, driver_->LastTransactionData()->offsets_size);
}

TEST_F(BinderTest, DeadEndpointTransaction) {
  Parcel data;
  uint32_t handle = BinderDriverStub::BAD_ENDPOINT;
  uint32_t code = 0;
  bool one_way = true;
  int ret;

  ret = BinderManagerInterface::Get()->Transact(handle, code, data, nullptr,
                                                one_way);
  EXPECT_EQ(ERROR_DEAD_ENDPOINT, ret);
  CheckTransaction(driver_->LastTransactionData(), handle, code, one_way);
}

TEST_F(BinderTest, OneWayDataTransaction) {
  Parcel data;
  uint32_t handle = 0x1111;
  uint32_t code = 0x2222;
  bool one_way = true;
  int ret;

  data.WriteInt32(100);
  data.WriteString("Yet Another IPC...");
  ret = BinderManagerInterface::Get()->Transact(handle, code, data, nullptr,
                                                one_way);
  EXPECT_EQ(SUCCESS, ret);
  CheckTransaction(driver_->LastTransactionData(), handle, code, one_way);

  // Check the data sent to the driver is correct.
  EXPECT_EQ(data.Len(), driver_->LastTransactionData()->data_size);
  EXPECT_EQ(0, driver_->LastTransactionData()->offsets_size);
  ASSERT_NE(0, driver_->LastTransactionData()->data.ptr.buffer);
  EXPECT_EQ(0, memcmp(data.Data(),
                      reinterpret_cast<void*>(
                          driver_->LastTransactionData()->data.ptr.buffer),
                      data.Len()));
}

TEST_F(BinderTest, OneWayDataAndObjectsTransaction) {
  Parcel data;
  uint32_t handle = 0x1111;
  uint32_t code = 0x2222;
  bool one_way = true;
  int ret;

  data.WriteInt32(100);
  data.WriteString("Yet Another IPC...");
  data.WriteFd(10);
  data.WriteFd(20);
  ret = BinderManagerInterface::Get()->Transact(handle, code, data, nullptr,
                                                one_way);
  EXPECT_EQ(SUCCESS, ret);
  CheckTransaction(driver_->LastTransactionData(), handle, code, one_way);

  // Check that the data and offset buffers sent to the driver are correct.
  EXPECT_EQ(data.Len(), driver_->LastTransactionData()->data_size);
  EXPECT_EQ(data.ObjectCount() * sizeof(binder_size_t),
            driver_->LastTransactionData()->offsets_size);
  ASSERT_NE(0, driver_->LastTransactionData()->data.ptr.buffer);
  EXPECT_EQ(0, memcmp(data.Data(),
                      reinterpret_cast<void*>(
                          driver_->LastTransactionData()->data.ptr.buffer),
                      data.Len()));
  ASSERT_NE(0, driver_->LastTransactionData()->data.ptr.offsets);
  EXPECT_EQ(0, memcmp(data.ObjectData(),
                      reinterpret_cast<void*>(
                          driver_->LastTransactionData()->data.ptr.offsets),
                      data.ObjectCount() * sizeof(binder_size_t)));
}

TEST_F(BinderTest, TwoWayTransaction) {
  Parcel data;
  Parcel reply;
  uint32_t handle = BinderDriverStub::GOOD_ENDPOINT;
  uint32_t code = 0;
  bool one_way = false;
  int ret;

  ret = BinderManagerInterface::Get()->Transact(handle, code, data, &reply,
                                                one_way);
  EXPECT_EQ(SUCCESS, ret);
  CheckTransaction(driver_->LastTransactionData(), handle, code, one_way);

  int val;
  EXPECT_TRUE(reply.ReadInt32(&val));
  EXPECT_EQ(BinderDriverStub::kReplyVal, val);

  std::string str;
  EXPECT_TRUE(reply.ReadString(&str));
  EXPECT_EQ(BinderDriverStub::kReplyString, str);

  EXPECT_TRUE(reply.IsEmpty());

  // Check one-way works on a call that replies with data.
  ret = BinderManagerInterface::Get()->Transact(handle, code, data, nullptr,
                                                true);
  EXPECT_EQ(SUCCESS, ret);
}

TEST_F(BinderTest, Proxy) {
  EXPECT_EQ(0, driver_->GetRefCount(BinderDriverStub::GOOD_ENDPOINT));
  {
    BinderProxy proxy(BinderDriverStub::GOOD_ENDPOINT);
    EXPECT_EQ(BinderDriverStub::GOOD_ENDPOINT, proxy.handle());
    EXPECT_EQ(1, driver_->GetRefCount(BinderDriverStub::GOOD_ENDPOINT));
    EXPECT_TRUE(driver_->IsDeathRegistered(reinterpret_cast<uintptr_t>(&proxy),
                                           BinderDriverStub::GOOD_ENDPOINT));

    BinderProxy proxy2(BinderDriverStub::GOOD_ENDPOINT);
    EXPECT_EQ(2, driver_->GetRefCount(BinderDriverStub::GOOD_ENDPOINT));
    EXPECT_TRUE(driver_->IsDeathRegistered(reinterpret_cast<uintptr_t>(&proxy2),
                                           BinderDriverStub::GOOD_ENDPOINT));
  }
  EXPECT_EQ(0, driver_->GetRefCount(BinderDriverStub::GOOD_ENDPOINT));
}

TEST_F(BinderTest, ProxyTransaction) {
  BinderProxy proxy(BinderDriverStub::GOOD_ENDPOINT);

  Parcel data;
  uint32_t code = 0x10;

  int ret = proxy.Transact(code, &data, nullptr, true);
  EXPECT_EQ(SUCCESS, ret);
  CheckTransaction(driver_->LastTransactionData(),
                   BinderDriverStub::GOOD_ENDPOINT, code, true);
}

TEST_F(BinderTest, ProxyDeathNotifcation) {
  BinderProxy proxy(BinderDriverStub::GOOD_ENDPOINT);
  EXPECT_FALSE(got_death_callback_);

  proxy.SetDeathCallback(
      base::Bind(&BinderTest_ProxyDeathNotifcation_Test::HandleBinderDeath,
                 weak_ptr_factory_.GetWeakPtr()));

  driver_->InjectDeathNotification((uintptr_t)&proxy);
  BinderManagerInterface::Get()->HandleEvent();

  EXPECT_TRUE(got_death_callback_);
}

TEST_F(BinderTest, HostOneWay) {
  EXPECT_FALSE(got_transact_callback_);

  HostTest host(base::Bind(&BinderTest_HostOneWay_Test::HandleTransaction,
                           weak_ptr_factory_.GetWeakPtr()));
  Parcel data;
  data.WriteInt32(0xDEAD);
  driver_->InjectTransaction((uintptr_t)&host, 10, data, true);
  BinderManagerInterface::Get()->HandleEvent();
  EXPECT_TRUE(got_transact_callback_);
}

TEST_F(BinderTest, HostTwoWay) {
  EXPECT_FALSE(got_transact_callback_);

  HostTest host(base::Bind(&BinderTest_HostTwoWay_Test::HandleTransaction,
                           weak_ptr_factory_.GetWeakPtr()));
  Parcel data;
  data.WriteInt32(0xDEAD);
  driver_->InjectTransaction((uintptr_t)&host, 10, data, false);
  BinderManagerInterface::Get()->HandleEvent();
  EXPECT_TRUE(got_transact_callback_);

  // Unfortunately the reply data has now gone out of scope,
  // but the size can still be validated.
  EXPECT_EQ(sizeof(uint32_t), driver_->LastTransactionData()->data_size);
}

}  // namespace protobinder
