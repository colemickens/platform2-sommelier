// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_MOCK_CURL_API_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_MOCK_CURL_API_H_

#include <string>

#include <chromeos/http/curl_api.h>
#include <gmock/gmock.h>

namespace chromeos {
namespace http {

class MockCurlInterface : public CurlInterface {
 public:
  MockCurlInterface() = default;

  MOCK_METHOD0(EasyInit, CURL*());
  MOCK_METHOD1(EasyCleanup, void(CURL*));
  MOCK_METHOD3(EasySetOptInt, CURLcode(CURL*, CURLoption, int));
  MOCK_METHOD3(EasySetOptStr, CURLcode(CURL*, CURLoption, const std::string&));
  MOCK_METHOD3(EasySetOptPtr, CURLcode(CURL*, CURLoption, void*));
  MOCK_METHOD3(EasySetOptCallback, CURLcode(CURL*, CURLoption, intptr_t));
  MOCK_METHOD3(EasySetOptOffT, CURLcode(CURL*, CURLoption, curl_off_t));
  MOCK_METHOD1(EasyPerform, CURLcode(CURL*));
  MOCK_CONST_METHOD3(EasyGetInfoInt, CURLcode(CURL*, CURLINFO, int*));
  MOCK_CONST_METHOD3(EasyGetInfoDbl, CURLcode(CURL*, CURLINFO, double*));
  MOCK_CONST_METHOD3(EasyGetInfoStr, CURLcode(CURL*, CURLINFO, std::string*));
  MOCK_CONST_METHOD1(EasyStrError, std::string(CURLcode));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCurlInterface);
};

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_MOCK_CURL_API_H_
