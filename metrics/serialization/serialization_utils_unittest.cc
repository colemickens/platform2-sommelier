// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "metrics/serialization/serialization_utils.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

#include "metrics/serialization/metric_sample.h"

namespace metrics {
namespace {

class SerializationUtilsTest : public testing::Test {
 protected:
  SerializationUtilsTest() {
    bool success = temporary_dir_.CreateUniqueTempDir();
    if (success) {
      base::FilePath dir_path = temporary_dir_.GetPath();
      filepath_ = dir_path.Append("chromeossampletest");
      filename_ = filepath_.value();
    }
  }

  void SetUp() override { base::DeleteFile(filepath_, false); }

  void TestSerialization(MetricSample* sample) {
    std::string serialized(sample->ToString());
    ASSERT_EQ('\0', serialized[serialized.length() - 1]);
    std::unique_ptr<MetricSample> deserialized =
        SerializationUtils::ParseSample(serialized);
    ASSERT_TRUE(deserialized.get());
    EXPECT_TRUE(sample->IsEqual(*deserialized.get()));
  }

  std::string filename_;
  base::ScopedTempDir temporary_dir_;
  base::FilePath filepath_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerializationUtilsTest);
};

TEST_F(SerializationUtilsTest, CrashSerializeTest) {
  TestSerialization(MetricSample::CrashSample("test").get());
}

TEST_F(SerializationUtilsTest, HistogramSerializeTest) {
  TestSerialization(
      MetricSample::HistogramSample("myhist", 13, 1, 100, 10).get());
}

TEST_F(SerializationUtilsTest, RepeatedSerializeTest) {
  TestSerialization(MetricSample::HistogramSample(
                        "myrepeatedhist", 26, 1, 100, 10, 1000).get());
}

TEST_F(SerializationUtilsTest, LinearSerializeTest) {
  TestSerialization(
      MetricSample::LinearHistogramSample("linearhist", 12, 30).get());
}

TEST_F(SerializationUtilsTest, SparseSerializeTest) {
  TestSerialization(MetricSample::SparseHistogramSample("mysparse", 30).get());
}

TEST_F(SerializationUtilsTest, UserActionSerializeTest) {
  TestSerialization(MetricSample::UserActionSample("myaction").get());
}

TEST_F(SerializationUtilsTest, IllegalNameAreFilteredTest) {
  std::unique_ptr<MetricSample> sample1 =
      MetricSample::SparseHistogramSample("no space", 10);
  std::unique_ptr<MetricSample> sample2 = MetricSample::LinearHistogramSample(
      base::StringPrintf("here%cbhe", '\0'), 1, 3);

  EXPECT_FALSE(SerializationUtils::WriteMetricToFile(*sample1.get(),
                                                     filename_));
  EXPECT_FALSE(SerializationUtils::WriteMetricToFile(*sample2.get(),
                                                     filename_));
  int64_t size = 0;

  ASSERT_TRUE(!PathExists(filepath_) || base::GetFileSize(filepath_, &size));

  EXPECT_EQ(0, size);
}

TEST_F(SerializationUtilsTest, BadInputIsCaughtTest) {
  std::string input(
      base::StringPrintf("sparsehistogram%cname foo%c", '\0', '\0'));
  EXPECT_EQ(NULL, MetricSample::ParseSparseHistogram(input).get());
}

TEST_F(SerializationUtilsTest, MessageSeparatedByZero) {
  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample("mycrash");

  SerializationUtils::WriteMetricToFile(*crash.get(), filename_);
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath_, &size));
  // 4 bytes for the size
  // 5 bytes for crash
  // 7 bytes for mycrash
  // 2 bytes for the \0
  // -> total of 18
  EXPECT_EQ(size, 18);
}

TEST_F(SerializationUtilsTest, MessagesTooLongAreDiscardedTest) {
  // Creates a message that is bigger than the maximum allowed size.
  // As we are adding extra character (crash, \0s, etc), if the name is
  // kMessageMaxLength long, it will be too long.
  std::string name(SerializationUtils::kMessageMaxLength, 'c');

  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample(name);
  EXPECT_FALSE(SerializationUtils::WriteMetricToFile(*crash.get(), filename_));
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath_, &size));
  EXPECT_EQ(0, size);
}

TEST_F(SerializationUtilsTest, ReadLongMessageTest) {
  base::File test_file(filepath_,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  std::string message(SerializationUtils::kMessageMaxLength + 1, 'c');

  int32_t message_size = message.length() + sizeof(int32_t);
  test_file.WriteAtCurrentPos(reinterpret_cast<const char*>(&message_size),
                              sizeof(message_size));
  test_file.WriteAtCurrentPos(message.c_str(), message.length());
  test_file.Close();

  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample("test");
  SerializationUtils::WriteMetricToFile(*crash.get(), filename_);

  std::vector<std::unique_ptr<MetricSample>> samples;
  SerializationUtils::ReadAndTruncateMetricsFromFile(
      filename_,
      &samples,
      SerializationUtils::kSampleBatchMaxLength);
  ASSERT_EQ(1u, samples.size());
  ASSERT_NE(nullptr, samples[0]);
  EXPECT_TRUE(crash->IsEqual(*samples[0]));
}

TEST_F(SerializationUtilsTest, WriteReadTest) {
  std::unique_ptr<MetricSample> hist =
      MetricSample::HistogramSample("myhist", 1, 2, 3, 4);
  std::unique_ptr<MetricSample> crash = MetricSample::CrashSample("mycrash");
  std::unique_ptr<MetricSample> lhist =
      MetricSample::LinearHistogramSample("linear", 1, 10);
  std::unique_ptr<MetricSample> shist =
      MetricSample::SparseHistogramSample("mysparse", 30);
  std::unique_ptr<MetricSample> action =
      MetricSample::UserActionSample("myaction");
  std::unique_ptr<MetricSample> repeatedhist =
      MetricSample::HistogramSample("myrepeatedhist", 1, 2, 3, 4, 10);

  SerializationUtils::WriteMetricToFile(*hist.get(), filename_);
  SerializationUtils::WriteMetricToFile(*crash.get(), filename_);
  SerializationUtils::WriteMetricToFile(*lhist.get(), filename_);
  SerializationUtils::WriteMetricToFile(*shist.get(), filename_);
  SerializationUtils::WriteMetricToFile(*action.get(), filename_);
  SerializationUtils::WriteMetricToFile(*repeatedhist.get(), filename_);
  std::vector<std::unique_ptr<MetricSample>> samples;
  SerializationUtils::ReadAndTruncateMetricsFromFile(
      filename_,
      &samples,
      SerializationUtils::kSampleBatchMaxLength);
  ASSERT_EQ(6, samples.size());
  for (const auto& sample : samples) {
    ASSERT_NE(nullptr, sample);
  }
  EXPECT_TRUE(hist->IsEqual(*samples[0]));
  EXPECT_TRUE(crash->IsEqual(*samples[1]));
  EXPECT_TRUE(lhist->IsEqual(*samples[2]));
  EXPECT_TRUE(shist->IsEqual(*samples[3]));
  EXPECT_TRUE(action->IsEqual(*samples[4]));
  EXPECT_TRUE(repeatedhist->IsEqual(*samples[5]));

  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(filepath_, &size));
  ASSERT_EQ(0, size);
}

// Test of batched upload.  Creates a metrics log with enough samples to
// trigger two uploads.
TEST_F(SerializationUtilsTest, BatchedUploadTest) {
  std::unique_ptr<MetricSample> hist =
      MetricSample::HistogramSample("Boring.Histogram", 1, 2, 3, 4);
  // The serialized MetricSample does not contain the header size (4 bytes for
  // the total sample length).
  size_t serialized_sample_length = hist->ToString().length() + 4;
  // Make the max batch size a multiple of the filesystem block size so we can
  // test the hole-punching optimization (maybe overkill, but fun).
  const size_t sample_batch_max_length = 10 * 4096;
  // Write enough samples for two passes.
  const int sample_count =
      1.5 * sample_batch_max_length / serialized_sample_length;

  for (int i = 0; i < sample_count; i++) {
    SerializationUtils::WriteMetricToFile(*hist.get(), filename_);
  }

  std::vector<std::unique_ptr<MetricSample>> samples;
  bool first_pass_status =
      SerializationUtils::ReadAndTruncateMetricsFromFile(
          filename_,
          &samples,
          sample_batch_max_length);

  ASSERT_FALSE(first_pass_status);  // means: more samples remain
  int first_pass_count = samples.size();
  ASSERT_LT(first_pass_count, sample_count);

  // There is nothing in the base library which returns the actual file
  // allocation (size - holes).
  struct stat stat_buf;
  // Check that stat() is successful.
  ASSERT_EQ(::stat(filename_.c_str(), &stat_buf), 0);
  // Check that the file is not truncated to zero.
  ASSERT_GT(stat_buf.st_size, 0);
  // Check that the file has holes.
  ASSERT_LT(stat_buf.st_blocks * 512, stat_buf.st_size);

  bool second_pass_status =
      SerializationUtils::ReadAndTruncateMetricsFromFile(
          filename_,
          &samples,
          sample_batch_max_length);

  ASSERT_TRUE(second_pass_status);  // no more samples.
  // Check that stat() is successful.
  ASSERT_EQ(::stat(filename_.c_str(), &stat_buf), 0);
  // Check that the file is empty.
  ASSERT_EQ(stat_buf.st_size, 0);
  // Check that we read all samples.
  ASSERT_EQ(samples.size(), sample_count);
}


}  // namespace
}  // namespace metrics
