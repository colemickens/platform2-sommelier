// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "feedback/feedback_uploader_http.h"

#include <chromeos/http/http_utils.h>
#include <chromeos/mime_utils.h>

namespace feedback {

FeedbackUploaderHttp::FeedbackUploaderHttp(
    const base::FilePath& path,
    base::SequencedWorkerPool* pool,
    const std::string& url) : FeedbackUploader(path, pool, url) {}

void FeedbackUploaderHttp::DispatchReport(const std::string& data) {
  chromeos::ErrorPtr error;
  auto response = chromeos::http::PostBinary(
      url_, data.data(), data.size(), chromeos::mime::application::kProtobuf,
      chromeos::http::Transport::CreateDefault(), &error);
  if (response) {
    LOG(INFO) << "Sending feedback: successful";
    UpdateUploadTimer();
  } else {
    LOG(WARNING) << "Sending feedback: failed with error "
                 << error->GetMessage() << ", retrying";
    RetryReport(data);
  }
}

}  // namespace feedback
