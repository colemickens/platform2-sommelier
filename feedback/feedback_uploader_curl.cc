// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "feedback/feedback_uploader_curl.h"

#include <curl/curl.h>

namespace {
const char kProtobufContentType[] = "Content-Type: application/x-protobuf";
}

namespace feedback {
FeedbackUploaderCurl::FeedbackUploaderCurl(
    const base::FilePath& path,
    base::SequencedWorkerPool* pool,
    const std::string& url) : FeedbackUploader(path, pool, url) {}

FeedbackUploaderCurl::~FeedbackUploaderCurl() {}

void FeedbackUploaderCurl::DispatchReport(const std::string& data) {
  CURL* curl = curl_easy_init();
  CHECK(curl);
  curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());

  curl_slist* hdrs = curl_slist_append(NULL, kProtobufContentType);
  CHECK(hdrs) << "curl hdr list allocation failed";
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

  CURLcode code = curl_easy_perform(curl);
  if (code == CURLE_OK) {
    LOG(INFO) << "Sending feedback: successful";
    UpdateUploadTimer();
  } else {
    LOG(WARNING) << "Sending feedback: failed with status " << code
                 << ", retrying";
    RetryReport(data);
  }
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
}
}  // namespace feedback
