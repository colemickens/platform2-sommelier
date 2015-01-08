// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_transport_curl.h>

#include <chromeos/http/http_request.h>
#include <chromeos/http/mock_curl_api.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::InSequence;
using testing::Return;

namespace chromeos {
namespace http {
namespace curl {

class HttpCurlTransportTest : public testing::Test {
 public:
  void SetUp() override {
    curl_api_ = std::make_shared<MockCurlInterface>();
    transport_ = std::make_shared<Transport>(curl_api_);
    handle_ = reinterpret_cast<CURL*>(100);  // Mock handle value.
  }

  void TearDown() override {
    transport_.reset();
    curl_api_.reset();
  }

 protected:
  std::shared_ptr<MockCurlInterface> curl_api_;
  std::shared_ptr<Transport> transport_;
  CURL* handle_{nullptr};
};

TEST_F(HttpCurlTransportTest, RequestGet) {
  InSequence seq;
  EXPECT_CALL(*curl_api_, EasyInit()).WillOnce(Return(handle_));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_URL, "http://foo.bar/get"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_USERAGENT, "User Agent"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_REFERER, "http://foo.bar/baz"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptInt(handle_, CURLOPT_HTTPGET, 1))
      .WillOnce(Return(CURLE_OK));
  auto connection = transport_->CreateConnection(
      "http://foo.bar/get", request_type::kGet, {}, "User Agent",
      "http://foo.bar/baz", nullptr);
  EXPECT_NE(nullptr, connection.get());

  EXPECT_CALL(*curl_api_, EasyCleanup(handle_)).Times(1);
  connection.reset();
}

TEST_F(HttpCurlTransportTest, RequestHead) {
  InSequence seq;
  EXPECT_CALL(*curl_api_, EasyInit()).WillOnce(Return(handle_));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_URL, "http://foo.bar/head"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptInt(handle_, CURLOPT_NOBODY, 1))
      .WillOnce(Return(CURLE_OK));
  auto connection = transport_->CreateConnection(
      "http://foo.bar/head", request_type::kHead, {}, "", "", nullptr);
  EXPECT_NE(nullptr, connection.get());

  EXPECT_CALL(*curl_api_, EasyCleanup(handle_)).Times(1);
  connection.reset();
}

TEST_F(HttpCurlTransportTest, RequestPut) {
  InSequence seq;
  EXPECT_CALL(*curl_api_, EasyInit()).WillOnce(Return(handle_));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_URL, "http://foo.bar/put"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptInt(handle_, CURLOPT_UPLOAD, 1))
      .WillOnce(Return(CURLE_OK));
  auto connection = transport_->CreateConnection(
      "http://foo.bar/put", request_type::kPut, {}, "", "", nullptr);
  EXPECT_NE(nullptr, connection.get());

  EXPECT_CALL(*curl_api_, EasyCleanup(handle_)).Times(1);
  connection.reset();
}

TEST_F(HttpCurlTransportTest, RequestPost) {
  InSequence seq;
  EXPECT_CALL(*curl_api_, EasyInit()).WillOnce(Return(handle_));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_URL, "http://www.foo.bar/post"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptInt(handle_, CURLOPT_POST, 1))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptPtr(handle_, CURLOPT_POSTFIELDS, nullptr))
      .WillOnce(Return(CURLE_OK));
  auto connection = transport_->CreateConnection(
      "http://www.foo.bar/post", request_type::kPost, {}, "", "", nullptr);
  EXPECT_NE(nullptr, connection.get());

  EXPECT_CALL(*curl_api_, EasyCleanup(handle_)).Times(1);
  connection.reset();
}

TEST_F(HttpCurlTransportTest, RequestPatch) {
  InSequence seq;
  EXPECT_CALL(*curl_api_, EasyInit()).WillOnce(Return(handle_));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_URL, "http://www.foo.bar/patch"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptInt(handle_, CURLOPT_POST, 1))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptPtr(handle_, CURLOPT_POSTFIELDS, nullptr))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptStr(handle_, CURLOPT_CUSTOMREQUEST,
                                        request_type::kPatch))
      .WillOnce(Return(CURLE_OK));
  auto connection = transport_->CreateConnection(
      "http://www.foo.bar/patch", request_type::kPatch, {}, "", "", nullptr);
  EXPECT_NE(nullptr, connection.get());

  EXPECT_CALL(*curl_api_, EasyCleanup(handle_)).Times(1);
  connection.reset();
}

TEST_F(HttpCurlTransportTest, CurlFailure) {
  InSequence seq;
  EXPECT_CALL(*curl_api_, EasyInit()).WillOnce(Return(handle_));
  EXPECT_CALL(*curl_api_,
              EasySetOptStr(handle_, CURLOPT_URL, "http://foo.bar/get"))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*curl_api_, EasySetOptInt(handle_, CURLOPT_HTTPGET, 1))
      .WillOnce(Return(CURLE_OUT_OF_MEMORY));
  EXPECT_CALL(*curl_api_, EasyStrError(CURLE_OUT_OF_MEMORY))
      .WillOnce(Return("Out of Memory"));
  EXPECT_CALL(*curl_api_, EasyCleanup(handle_)).Times(1);
  ErrorPtr error;
  auto connection = transport_->CreateConnection(
      "http://foo.bar/get", request_type::kGet, {}, "", "", &error);

  EXPECT_EQ(nullptr, connection.get());
  EXPECT_EQ("curl_error", error->GetDomain());
  EXPECT_EQ(std::to_string(CURLE_OUT_OF_MEMORY), error->GetCode());
  EXPECT_EQ("Out of Memory", error->GetMessage());
}

}  // namespace curl
}  // namespace http
}  // namespace chromeos
