// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/examples/ubuntu/curl_http_client.h"

#include <base/bind.h>
#include <curl/curl.h>
#include <weave/task_runner.h>

namespace weave {
namespace examples {

namespace {

struct ResponseImpl : public HttpClient::Response {
  int GetStatusCode() const { return status; }
  std::string GetContentType() const { return content_type; }
  const std::string& GetData() const { return data; }

  int status;
  std::string content_type;
  std::string data;
};

size_t WriteFunction(void* contents, size_t size, size_t nmemb, void* userp) {
  static_cast<std::string*>(userp)
      ->append(static_cast<const char*>(contents), size * nmemb);
  return size * nmemb;
}

}  // namespace

CurlHttpClient::CurlHttpClient(TaskRunner* task_runner)
    : task_runner_{task_runner} {}

std::unique_ptr<HttpClient::Response> CurlHttpClient::SendRequestAndBlock(
    const std::string& method,
    const std::string& url,
    const Headers& headers,
    const std::string& data,
    ErrorPtr* error) {
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl{curl_easy_init(),
                                                           &curl_easy_cleanup};
  CHECK(curl);

  if (method == "GET") {
    CHECK_EQ(CURLE_OK, curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L));
  } else if (method == "POST") {
    CHECK_EQ(CURLE_OK, curl_easy_setopt(curl.get(), CURLOPT_HTTPPOST, 1L));
  } else {
    CHECK_EQ(CURLE_OK, curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST,
                                        method.c_str()));
  }

  CHECK_EQ(CURLE_OK, curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str()));

  curl_slist* chunk = nullptr;
  for (const auto& h : headers)
    chunk = curl_slist_append(chunk, (h.first + ": " + h.second).c_str());

  CHECK_EQ(CURLE_OK, curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, chunk));

  if (!data.empty() || method == "POST") {
    CHECK_EQ(CURLE_OK,
             curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, data.c_str()));
  }

  std::unique_ptr<ResponseImpl> response{new ResponseImpl};
  CHECK_EQ(CURLE_OK,
           curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, &WriteFunction));
  CHECK_EQ(CURLE_OK,
           curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response->data));
  CHECK_EQ(CURLE_OK, curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION,
                                      &WriteFunction));
  CHECK_EQ(CURLE_OK, curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA,
                                      &response->content_type));

  CURLcode res = curl_easy_perform(curl.get());
  if (chunk)
    curl_slist_free_all(chunk);

  if (res != CURLE_OK) {
    Error::AddTo(error, FROM_HERE, "curl", "curl_easy_perform_error",
                 curl_easy_strerror(res));
    return nullptr;
  }

  const std::string kContentType = "\r\nContent-Type:";
  auto pos = response->content_type.find(kContentType);
  if (pos == std::string::npos) {
    Error::AddTo(error, FROM_HERE, "curl", "no_content_header",
                 "Content-Type header is missing");
    return nullptr;
  }
  pos += kContentType.size();
  auto pos_end = response->content_type.find("\r\n", pos);
  if (pos_end == std::string::npos) {
    pos_end = response->content_type.size();
  }

  response->content_type = response->content_type.substr(pos, pos_end);

  CHECK_EQ(CURLE_OK, curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE,
                                       &response->status));
  return std::move(response);
}

int CurlHttpClient::SendRequest(const std::string& method,
                                const std::string& url,
                                const Headers& headers,
                                const std::string& data,
                                const SuccessCallback& success_callback,
                                const ErrorCallback& error_callback) {
  ++request_id_;
  ErrorPtr error;
  auto response = SendRequestAndBlock(method, url, headers, data, &error);
  if (response) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&CurlHttpClient::RunSuccessCallback,
                              weak_ptr_factory_.GetWeakPtr(), success_callback,
                              request_id_, base::Passed(&response)),
        {});
    return request_id_;
  }
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&CurlHttpClient::RunErrorCallback,
                            weak_ptr_factory_.GetWeakPtr(), error_callback,
                            request_id_, base::Passed(&error)),
      {});
}

void CurlHttpClient::RunSuccessCallback(const SuccessCallback& success_callback,
                                        int id,
                                        std::unique_ptr<Response> response) {
  success_callback.Run(id, *response);
}

void CurlHttpClient::RunErrorCallback(const ErrorCallback& error_callback,
                                      int id,
                                      ErrorPtr error) {
  error_callback.Run(id, error.get());
}

}  // namespace examples
}  // namespace weave
