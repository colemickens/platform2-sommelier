// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "feedback/feedback_service.h"

#include <utime.h>

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/guid.h>
#include <base/strings/stringprintf.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/proto/extension.pb.h"

namespace feedback {

namespace {

static const int kMaxPoolThreads = 1;
static const char kPoolName[] = "FeedbackWorkerPool";
static const char kFeedbackReportPath[] = "Feedback Reports";
static const int kTestProductId = 84;

}  // namespace

class MockFeedbackUploader : public feedback::FeedbackUploader {
 public:
  MockFeedbackUploader(
      const base::FilePath& path,
      base::SequencedWorkerPool* pool) : FeedbackUploader(path, pool) {}

  MOCK_METHOD1(DispatchReport, void(const std::string&));
};

class MockFeedbackUploaderQueue : public MockFeedbackUploader {
 public:
  MockFeedbackUploaderQueue(
      const base::FilePath& path,
      base::SequencedWorkerPool* pool) : MockFeedbackUploader(path, pool) {}

  MOCK_METHOD1(QueueReport, void(const std::string&));
};

class FailedFeedbackUploader : public MockFeedbackUploader {
 public:
  FailedFeedbackUploader(
      const base::FilePath& path,
      base::SequencedWorkerPool* pool) : MockFeedbackUploader(path, pool) {}

  void DispatchReport(const std::string& data) {
    MockFeedbackUploader::DispatchReport(data);
    RetryReport(data);
  }

  feedback::FeedbackReport* GetFirstReport() {
    scoped_refptr<feedback::FeedbackReport> report = reports_queue_.top();
    return report.get();
  }
};

class FeedbackServiceTest : public testing::Test {
 public:
  FeedbackServiceTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

  static void CallbackFeedbackResult(bool expected_result, bool result,
                                     const std::string& err) {
    EXPECT_EQ(result, expected_result);
  }

  userfeedback::ExtensionSubmit GetBaseReport() {
    userfeedback::ExtensionSubmit report;
    report.mutable_common_data();
    report.mutable_web_data();
    report.set_type_id(0);
    report.set_product_id(kTestProductId);
    return report;
  }

  void WaitOnPool() {
    pool_->FlushForTesting();
  }

 protected:
  virtual void SetUp() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateDirectory(temp_dir_.GetPath().Append(kFeedbackReportPath));

    pool_ = new base::SequencedWorkerPool(kMaxPoolThreads, kPoolName,
                                          base::TaskPriority::BACKGROUND);
  }

  virtual void TearDown() {
    WaitOnPool();
    pool_->Shutdown();
  }

  base::ScopedTempDir temp_dir_;
  base::MessageLoop message_loop_;
  scoped_refptr<base::SequencedWorkerPool> pool_;
};

TEST_F(FeedbackServiceTest, SendFeedback) {
  MockFeedbackUploaderQueue uploader(temp_dir_.GetPath(), pool_.get());
  std::string data;
  userfeedback::ExtensionSubmit report = GetBaseReport();
  report.SerializeToString(&data);
  EXPECT_CALL(uploader, QueueReport(data)).Times(1);

  scoped_refptr<FeedbackService> svc = new FeedbackService(&uploader);

  svc->SendFeedback(report,
      base::Bind(&FeedbackServiceTest::CallbackFeedbackResult, true));
}

TEST_F(FeedbackServiceTest, DispatchTest) {
  MockFeedbackUploader uploader(temp_dir_.GetPath(), pool_.get());
  std::string data;
  userfeedback::ExtensionSubmit report = GetBaseReport();
  report.SerializeToString(&data);
  EXPECT_CALL(uploader, DispatchReport(data)).Times(1);

  scoped_refptr<FeedbackService> svc = new FeedbackService(&uploader);

  svc->SendFeedback(report,
      base::Bind(&FeedbackServiceTest::CallbackFeedbackResult, true));
}

TEST_F(FeedbackServiceTest, UploadFailure) {
  FailedFeedbackUploader uploader(temp_dir_.GetPath(), pool_.get());
  std::string data;
  userfeedback::ExtensionSubmit report = GetBaseReport();
  report.SerializeToString(&data);
  EXPECT_CALL(uploader, DispatchReport(data)).Times(1);

  scoped_refptr<FeedbackService> svc = new FeedbackService(&uploader);

  svc->SendFeedback(report,
      base::Bind(&FeedbackServiceTest::CallbackFeedbackResult, true));
  WaitOnPool();

  // Verify that this got put back on the queue.
  EXPECT_TRUE(uploader.GetFirstReport() != nullptr);
  EXPECT_EQ(uploader.GetFirstReport()->data(), data);
}

}  // namespace feedback

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  // Some libchrome calls need this.
  base::AtExitManager at_exit_manager;

  return RUN_ALL_TESTS();
}
