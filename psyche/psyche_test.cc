// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyche.h"

#include <gtest/gtest.h>

namespace psyche {

class PsycheTest : public testing::Test {
 public:
  PsycheTest() {}
  virtual ~PsycheTest() {}
  void SetUp() {
    psyche_.Init();
    service_ = std::string("service");
    service_is_running_ = false;
    service_is_ephemeral_ = true;
    EXPECT_TRUE(psyche_.AddService(service_,
                                   service_is_running_,
                                   service_is_ephemeral_));
    service_size_ = GetServiceInfo().size();
  }

  std::map<std::string, ServiceInfo> GetServiceInfo() {
    return psyche_.service_info_map_;
  }

 protected:
  Psyche psyche_;
  std::string service_;
  bool service_is_running_;
  bool service_is_ephemeral_;
  size_t service_size_;
};

TEST_F(PsycheTest, AddServiceSuccess) {
  std::string test_service("test_service");
  EXPECT_TRUE(psyche_.AddService(test_service, false, true));
  auto services = GetServiceInfo();
  EXPECT_EQ(services.size(), (service_size_ + 1));
  EXPECT_NE(services.find(test_service), services.end());
}

TEST_F(PsycheTest, AddServiceDuplicateFail) {
  EXPECT_FALSE(psyche_.AddService(service_, false, true));
  auto services = GetServiceInfo();
  EXPECT_EQ(services.size(), service_size_);
  EXPECT_NE(services.find(service_), services.end());
}

TEST_F(PsycheTest, FindServiceSuccess) {
  ServiceInfo info;
  EXPECT_TRUE(psyche_.FindServiceStatus(service_, &info));
  EXPECT_EQ(service_is_running_, info.is_running);
  EXPECT_EQ(service_is_ephemeral_, info.is_ephemeral);
}

TEST_F(PsycheTest, FindServiceNonexistentFail) {
  ServiceInfo info;
  EXPECT_FALSE(psyche_.FindServiceStatus("nonexistent_service", &info));
}

}  // namespace psyche
