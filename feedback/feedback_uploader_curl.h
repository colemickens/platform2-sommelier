// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FEEDBACK_FEEDBACK_UPLOADER_CURL_H_
#define FEEDBACK_FEEDBACK_UPLOADER_CURL_H_

#include "components/feedback/feedback_uploader.h"

#include <string>

namespace feedback {

class FeedbackUploaderCurl : public feedback::FeedbackUploader {
 public:
  FeedbackUploaderCurl(const base::FilePath& path,
                       base::SequencedWorkerPool* pool,
                       const std::string& url);
  ~FeedbackUploaderCurl();

 private:
  friend class FeedbackServiceTest;

  void DispatchReport(const std::string& data) override;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderCurl);
};

}  // namespace feedback

#endif  // FEEDBACK_FEEDBACK_UPLOADER_CURL_H_
