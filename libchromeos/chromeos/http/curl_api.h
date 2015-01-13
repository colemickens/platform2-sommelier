// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_CURL_API_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_CURL_API_H_

#include <curl/curl.h>

#include <string>

#include <base/macros.h>
#include <chromeos/chromeos_export.h>

namespace chromeos {
namespace http {

// Abstract wrapper around libcurl C API that allows us to mock it out in tests.
class CurlInterface {
 public:
  CurlInterface() = default;
  virtual ~CurlInterface() = default;

  // Wrapper around curl_easy_init().
  virtual CURL* EasyInit() = 0;

  // Wrapper around curl_easy_cleanup().
  virtual void EasyCleanup(CURL* curl) = 0;

  // Wrappers around curl_easy_setopt().
  virtual CURLcode EasySetOptInt(CURL* curl, CURLoption option, int value) = 0;
  virtual CURLcode EasySetOptStr(CURL* curl,
                                 CURLoption option,
                                 const std::string& value) = 0;
  virtual CURLcode EasySetOptPtr(CURL* curl,
                                 CURLoption option,
                                 void* value) = 0;
  virtual CURLcode EasySetOptCallback(CURL* curl,
                                      CURLoption option,
                                      intptr_t address) = 0;
  virtual CURLcode EasySetOptOffT(CURL* curl,
                                  CURLoption option,
                                  curl_off_t value) = 0;

  // A type-safe wrapper around function callback options.
  template<typename R, typename... Args>
  inline CURLcode EasySetOptCallback(CURL* curl,
                                     CURLoption option,
                                     R(*callback)(Args...)) {
    return EasySetOptCallback(
        curl, option, reinterpret_cast<intptr_t>(callback));
  }

  // Wrapper around curl_easy_perform().
  virtual CURLcode EasyPerform(CURL* curl) = 0;

  // Wrappers around curl_easy_getinfo().
  virtual CURLcode EasyGetInfoInt(CURL* curl,
                                  CURLINFO info,
                                  int* value) const = 0;
  virtual CURLcode EasyGetInfoDbl(CURL* curl,
                                  CURLINFO info,
                                  double* value) const = 0;
  virtual CURLcode EasyGetInfoStr(CURL* curl,
                                  CURLINFO info,
                                  std::string* value) const = 0;

  // Wrapper around curl_easy_strerror().
  virtual std::string EasyStrError(CURLcode code) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CurlInterface);
};

class CHROMEOS_EXPORT CurlApi : public CurlInterface {
 public:
  CurlApi();
  ~CurlApi() override;

  // Wrapper around curl_easy_init().
  CURL* EasyInit() override;

  // Wrapper around curl_easy_cleanup().
  void EasyCleanup(CURL* curl) override;

  // Wrappers around curl_easy_setopt().
  CURLcode EasySetOptInt(CURL* curl, CURLoption option, int value) override;
  CURLcode EasySetOptStr(CURL* curl,
                         CURLoption option,
                         const std::string& value) override;
  CURLcode EasySetOptPtr(CURL* curl, CURLoption option, void* value) override;
  CURLcode EasySetOptCallback(CURL* curl,
                              CURLoption option,
                              intptr_t address) override;
  CURLcode EasySetOptOffT(CURL* curl,
                          CURLoption option,
                          curl_off_t value) override;

  // Wrapper around curl_easy_perform().
  CURLcode EasyPerform(CURL* curl) override;

  // Wrappers around curl_easy_getinfo().
  CURLcode EasyGetInfoInt(CURL* curl, CURLINFO info, int* value) const override;
  CURLcode EasyGetInfoDbl(CURL* curl,
                          CURLINFO info,
                          double* value) const override;
  CURLcode EasyGetInfoStr(CURL* curl,
                          CURLINFO info,
                          std::string* value) const override;

  // Wrapper around curl_easy_strerror().
  std::string EasyStrError(CURLcode code) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CurlApi);
};

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_CURL_API_H_
