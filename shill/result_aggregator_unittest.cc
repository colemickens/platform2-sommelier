// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/result_aggregator.h"

#include <base/bind.h>
#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shill {

using base::Bind;
using base::Unretained;

class ResultAggregatorTest : public ::testing::Test {
 public:
  ResultAggregatorTest()
      : aggregator_(new ResultAggregator(
            Bind(&ResultAggregatorTest::ReportResult, Unretained(this)))) {}
  virtual ~ResultAggregatorTest() {}

  virtual void TearDown() {
    aggregator_ = NULL;  // Ensure ReportResult is invoked before our dtor.
  }

 protected:
  MOCK_METHOD1(ReportResult, void(const Error &));
  scoped_refptr<ResultAggregator> aggregator_;
};

class ResultGenerator {
 public:
  explicit ResultGenerator(const scoped_refptr<ResultAggregator> &aggregator)
      : aggregator_(aggregator) {}
  ~ResultGenerator() {}

  void GenerateResult(const Error::Type error_type) {
    aggregator_->ReportResult(Error(error_type));
  }

 private:
  scoped_refptr<ResultAggregator> aggregator_;
  DISALLOW_COPY_AND_ASSIGN(ResultGenerator);
};

MATCHER_P(ErrorType, type, "") {
  return arg.type() == type;
}

TEST_F(ResultAggregatorTest, Unused) {
  EXPECT_CALL(*this, ReportResult(ErrorType(Error::kSuccess))).Times(0);
}

TEST_F(ResultAggregatorTest, BothSucceed) {
  EXPECT_CALL(*this, ReportResult(ErrorType(Error::kSuccess)));
  ResultGenerator first_generator(aggregator_);
  ResultGenerator second_generator(aggregator_);
  first_generator.GenerateResult(Error::kSuccess);
  second_generator.GenerateResult(Error::kSuccess);
}

TEST_F(ResultAggregatorTest, FirstFails) {
  EXPECT_CALL(*this, ReportResult(ErrorType(Error::kOperationTimeout)));
  ResultGenerator first_generator(aggregator_);
  ResultGenerator second_generator(aggregator_);
  first_generator.GenerateResult(Error::kOperationTimeout);
  second_generator.GenerateResult(Error::kSuccess);
}

TEST_F(ResultAggregatorTest, SecondFails) {
  EXPECT_CALL(*this, ReportResult(ErrorType(Error::kOperationTimeout)));
  ResultGenerator first_generator(aggregator_);
  ResultGenerator second_generator(aggregator_);
  first_generator.GenerateResult(Error::kSuccess);
  second_generator.GenerateResult(Error::kOperationTimeout);
}

TEST_F(ResultAggregatorTest, BothFail) {
  EXPECT_CALL(*this, ReportResult(ErrorType(Error::kOperationTimeout)));
  ResultGenerator first_generator(aggregator_);
  ResultGenerator second_generator(aggregator_);
  first_generator.GenerateResult(Error::kOperationTimeout);
  second_generator.GenerateResult(Error::kPermissionDenied);
}

}  // namespace shill
