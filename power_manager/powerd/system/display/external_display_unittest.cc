// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "power_manager/powerd/system/display/external_display.h"

namespace power_manager {
namespace system {

namespace {

// Returns a two-character hexadecimal representation of |byte|.
std::string Hex(uint8 byte) { return base::HexEncode(&byte, 1); }

// Test implementation of ExternalDisplay::Delegate.
class TestDelegate : public ExternalDisplay::Delegate {
 public:
  TestDelegate() : report_write_failure_(false), report_read_failure_(false) {}
  virtual ~TestDelegate() {}

  void set_reply_message(const std::vector<uint8>& message) {
    reply_message_ = message;
  }
  void set_report_write_failure(bool failure) {
    report_write_failure_ = failure;
  }
  void set_report_read_failure(bool failure) { report_read_failure_ = failure; }

  // Returns the single message present in |sent_messages_|, if any, and clears
  // the vector. Crashes if multiple messages are present.
  std::string PopSentMessage() {
    std::string message;
    CHECK_LE(sent_messages_.size(), 1);
    if (!sent_messages_.empty())
      message = sent_messages_[0];
    sent_messages_.clear();
    return message;
  }

  // ExternalDisplay::Delegate implementation:
  virtual std::string GetName() const OVERRIDE { return "i2c-test"; }

  virtual bool PerformI2COperation(struct i2c_rdwr_ioctl_data* data) OVERRIDE {
    // Check that the passed-in data is remotely sane.
    CHECK(data);
    CHECK_EQ(data->nmsgs, 1);
    struct i2c_msg* const i2c_message = data->msgs;
    CHECK(i2c_message);
    CHECK(i2c_message->buf);
    uint8* const message = i2c_message->buf;
    const size_t message_length = i2c_message->len;
    CHECK(message);
    CHECK_GT(message_length, 0);

    if (i2c_message->addr != ExternalDisplay::kDdcI2CAddress) {
      LOG(ERROR) << "Ignoring operation with I2C address " << i2c_message->addr;
      return false;
    }

    // Write request.
    if (i2c_message->flags == 0) {
      if (report_write_failure_)
        return false;

      sent_messages_.push_back(base::HexEncode(message, message_length));
      return true;
    }

    // Read request.
    if (i2c_message->flags == I2C_M_RD) {
      if (report_read_failure_)
        return false;

      if (message_length != reply_message_.size()) {
        LOG(ERROR) << "Got request to read " << message_length << " byte(s); "
                   << "expected " << reply_message_.size();
        reply_message_.clear();
        return false;
      }
      memcpy(message, &(reply_message_[0]), message_length);
      reply_message_.clear();
      return true;
    }

    LOG(ERROR) << "Ignoring operation with I2C flags " << i2c_message->flags;
    return false;
  }

 private:
  // Sent messages, converted to hexadecimal strings, in the order they were
  // transmitted.
  std::vector<std::string> sent_messages_;

  // Message that should be returned in response to read requests.
  // The message will be cleared after the next read request.
  std::vector<uint8> reply_message_;

  // True if either writes or reads should report failure.
  bool report_write_failure_;
  bool report_read_failure_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

class ExternalDisplayTest : public testing::Test {
 public:
  ExternalDisplayTest()
      : delegate_(new TestDelegate),
        display_(scoped_ptr<ExternalDisplay::Delegate>(delegate_)),
        test_api_(&display_) {
    request_brightness_message_ =
        // Message header.
        Hex(ExternalDisplay::kDdcHostAddress) +
        Hex(ExternalDisplay::kDdcMessageBodyLengthMask | 2) +
        // Message body.
        Hex(ExternalDisplay::kDdcGetCommand) +
        Hex(ExternalDisplay::kDdcBrightnessIndex) +
        // Checksum byte.
        Hex(ExternalDisplay::kDdcDisplayAddress ^
            ExternalDisplay::kDdcHostAddress ^
            (ExternalDisplay::kDdcMessageBodyLengthMask | 2) ^
            ExternalDisplay::kDdcGetCommand ^
            ExternalDisplay::kDdcBrightnessIndex);
  }
  virtual ~ExternalDisplayTest() {}

 protected:
  // Updates the checksum byte that's already present at the end of |message|.
  void UpdateChecksum(uint8 starting_value, std::vector<uint8>* message) {
    uint8 checksum = starting_value;
    for (size_t i = 0; i < message->size() - 1; ++i)
      checksum ^= (*message)[i];
    (*message)[message->size() - 1] = checksum;
  }

  // Generate a reply to a request to get the brightness, suitable for passing
  // to TestDelegate::set_reply_message().
  std::vector<uint8> GetBrightnessReply(uint16 current_brightness,
                                        uint16 max_brightness) {
    std::vector<uint8> message;
    // Message header.
    message.push_back(ExternalDisplay::kDdcDisplayAddress);
    message.push_back(ExternalDisplay::kDdcMessageBodyLengthMask | 8);
    // Message body.
    message.push_back(ExternalDisplay::kDdcGetReplyCommand);
    message.push_back(0x0);  // Result code.
    message.push_back(ExternalDisplay::kDdcBrightnessIndex);
    message.push_back(0x0);  // VCP type code.
    message.push_back(max_brightness >> 8);
    message.push_back(max_brightness & 0xff);
    message.push_back(current_brightness >> 8);
    message.push_back(current_brightness & 0xff);
    // Checksum byte.
    message.push_back(0x0);
    UpdateChecksum(ExternalDisplay::kDdcVirtualHostAddress, &message);

    return message;
  }

  // Returns the string representation of the message that should be sent to set
  // the brightness to |brightness|.
  std::string GetSetBrightnessMessage(uint16 brightness) {
    const uint8 high_byte = brightness >> 8;
    const uint8 low_byte = brightness & 0xff;
    return
        // Message header.
        Hex(ExternalDisplay::kDdcHostAddress) +
        Hex(ExternalDisplay::kDdcMessageBodyLengthMask | 4) +
        // Message body.
        Hex(ExternalDisplay::kDdcSetCommand) +
        Hex(ExternalDisplay::kDdcBrightnessIndex) +
        Hex(high_byte) +
        Hex(low_byte) +
        // Checksum byte.
        Hex(ExternalDisplay::kDdcDisplayAddress ^
            ExternalDisplay::kDdcHostAddress ^
            (ExternalDisplay::kDdcMessageBodyLengthMask | 4) ^
            ExternalDisplay::kDdcSetCommand ^
            ExternalDisplay::kDdcBrightnessIndex ^
            high_byte ^
            low_byte);
  }

  // What a message requesting the display brightness should look like.
  std::string request_brightness_message_;

  TestDelegate* delegate_;  // weak pointer
  ExternalDisplay display_;
  ExternalDisplay::TestApi test_api_;
};

TEST_F(ExternalDisplayTest, BasicCommunication) {
  // Asking for brightness to be increased by 10% should result in a "get
  // brightness" request being sent.
  display_.AdjustBrightnessByPercent(10.0);
  EXPECT_EQ(request_brightness_message_, delegate_->PopSentMessage());

  // After the timer fires, the reply should be read and a request to set the
  // brightness to 60 should be sent.
  delegate_->set_reply_message(GetBrightnessReply(50, 100));
  EXPECT_EQ(ExternalDisplay::kDdcGetDelayMs,
            test_api_.GetTimerDelay().InMilliseconds());
  ASSERT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ(GetSetBrightnessMessage(60), delegate_->PopSentMessage());

  // Asking for more changes shouldn't result in requests being sent at first,
  // since no time has passed since the previous request. After the timer fires,
  // a new request should be sent containing both adjustments.
  display_.AdjustBrightnessByPercent(20.0);
  EXPECT_EQ("", delegate_->PopSentMessage());
  display_.AdjustBrightnessByPercent(5.0);
  EXPECT_EQ("", delegate_->PopSentMessage());
  EXPECT_EQ(ExternalDisplay::kDdcSetDelayMs,
            test_api_.GetTimerDelay().InMilliseconds());
  ASSERT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ(GetSetBrightnessMessage(85), delegate_->PopSentMessage());

  // The timer should fire again when it's safe to send another message, but
  // nothing should happen since there are no pending adjustments.
  EXPECT_EQ(ExternalDisplay::kDdcSetDelayMs,
            test_api_.GetTimerDelay().InMilliseconds());
  EXPECT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ("", delegate_->PopSentMessage());
  EXPECT_FALSE(test_api_.TriggerTimeout());

  // Let enough time pass for the cached brightness to be invalidated.
  // Asking for another adjustment should result in the brightness being
  // re-read.
  test_api_.AdvanceTime(base::TimeDelta::FromMilliseconds(
      ExternalDisplay::kCachedBrightnessValidMs + 10));
  display_.AdjustBrightnessByPercent(-10.0);
  EXPECT_EQ(request_brightness_message_, delegate_->PopSentMessage());

  // Pretend like the user decreased the brightness via physical buttons on the
  // monitor and reply that the current level is 30.
  delegate_->set_reply_message(GetBrightnessReply(30, 100));
  EXPECT_EQ(ExternalDisplay::kDdcGetDelayMs,
            test_api_.GetTimerDelay().InMilliseconds());
  ASSERT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ(GetSetBrightnessMessage(20), delegate_->PopSentMessage());
}

TEST_F(ExternalDisplayTest, InvalidBrightnessReplies) {
  // Each pair consists of a mangled reply to send and a human-readable
  // description of the test case.
  std::vector<std::pair<std::vector<uint8>, std::string> > test_cases;
  std::vector<uint8> reply = GetBrightnessReply(50, 100);
  reply[reply.size() - 1] += 1;
  test_cases.push_back(make_pair(reply, "incorrect checksum"));

  reply = GetBrightnessReply(50, 100);
  reply[0] += 1;
  UpdateChecksum(ExternalDisplay::kDdcVirtualHostAddress, &reply);
  test_cases.push_back(make_pair(reply, "incorrect source address"));

  reply = GetBrightnessReply(50, 100);
  reply[1] = ExternalDisplay::kDdcMessageBodyLengthMask | 9;  // Should be 8.
  UpdateChecksum(ExternalDisplay::kDdcVirtualHostAddress, &reply);
  test_cases.push_back(make_pair(reply, "incorrect body length"));

  reply = GetBrightnessReply(50, 100);
  reply[2] = ExternalDisplay::kDdcSetCommand;
  UpdateChecksum(ExternalDisplay::kDdcVirtualHostAddress, &reply);
  test_cases.push_back(make_pair(reply, "non-reply command"));

  reply = GetBrightnessReply(50, 100);
  reply[3] = 0x1;  // Should be 0x0 for success.
  UpdateChecksum(ExternalDisplay::kDdcVirtualHostAddress, &reply);
  test_cases.push_back(make_pair(reply, "non-zero result code"));

  reply = GetBrightnessReply(50, 100);
  reply[4] = ExternalDisplay::kDdcBrightnessIndex + 1;
  UpdateChecksum(ExternalDisplay::kDdcVirtualHostAddress, &reply);
  test_cases.push_back(make_pair(reply, "non-brightness feature index"));

  // Run through each test case, making sure that no subsequent request is sent
  // after the bogus reply is returned. The timer also shouldn't be rescheduled.
  for (size_t i = 0; i < test_cases.size(); ++i) {
    SCOPED_TRACE(test_cases[i].second);
    display_.AdjustBrightnessByPercent(10.0);
    ASSERT_EQ(request_brightness_message_, delegate_->PopSentMessage());
    delegate_->set_reply_message(test_cases[i].first);
    ASSERT_TRUE(test_api_.TriggerTimeout());
    EXPECT_EQ("", delegate_->PopSentMessage());
    EXPECT_FALSE(test_api_.TriggerTimeout());
  }
}

TEST_F(ExternalDisplayTest, CommunicationFailures) {
  // If the initial brightness request fails, the timer should be stopped.
  delegate_->set_report_write_failure(true);
  display_.AdjustBrightnessByPercent(10.0);
  EXPECT_FALSE(test_api_.TriggerTimeout());

  // Now let the initial request succeed but make the read fail. The timer
  // should be stopped.
  delegate_->set_report_write_failure(false);
  delegate_->set_report_read_failure(true);
  display_.AdjustBrightnessByPercent(10.0);
  EXPECT_EQ(request_brightness_message_, delegate_->PopSentMessage());
  ASSERT_TRUE(test_api_.TriggerTimeout());
  EXPECT_FALSE(test_api_.TriggerTimeout());

  // Let the initial request and read succeed, but make the attempt to change
  // the brightness fail. The timer should be stopped.
  delegate_->set_report_read_failure(false);
  display_.AdjustBrightnessByPercent(10.0);
  EXPECT_EQ(request_brightness_message_, delegate_->PopSentMessage());
  delegate_->set_report_write_failure(true);
  delegate_->set_reply_message(GetBrightnessReply(50, 100));
  ASSERT_TRUE(test_api_.TriggerTimeout());
  EXPECT_FALSE(test_api_.TriggerTimeout());

  // The previously-read brightness should still be cached, so another
  // adjustment attempt should be attempted immediately. Let it succeed this
  // time.
  delegate_->set_report_write_failure(false);
  display_.AdjustBrightnessByPercent(10.0);
  EXPECT_EQ(GetSetBrightnessMessage(60), delegate_->PopSentMessage());
}

TEST_F(ExternalDisplayTest, MinimumAndMaximumBrightness) {
  // A request to go below 0 should be capped.
  display_.AdjustBrightnessByPercent(-80.0);
  EXPECT_EQ(request_brightness_message_, delegate_->PopSentMessage());
  delegate_->set_reply_message(GetBrightnessReply(50, 80));
  ASSERT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ(GetSetBrightnessMessage(0), delegate_->PopSentMessage());

  // Now that the brightness is at 0, a request to decrease it further shouldn't
  // result in a message being sent to the display.
  display_.AdjustBrightnessByPercent(-10.0);
  EXPECT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ("", delegate_->PopSentMessage());

  // Requests above the maximum brightness should also be capped.
  display_.AdjustBrightnessByPercent(120.0);
  EXPECT_EQ(GetSetBrightnessMessage(80), delegate_->PopSentMessage());

  // Trying to increase the brightness further shouldn't do anything.
  display_.AdjustBrightnessByPercent(10.0);
  EXPECT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ("", delegate_->PopSentMessage());

  // Decrease the brightness a bit, and then check that if two adjustments that
  // cancel each other are requested before it's safe to send another request,
  // they cancel each other out and result in nothing being sent to the display.
  display_.AdjustBrightnessByPercent(-10.0);
  EXPECT_EQ(GetSetBrightnessMessage(72), delegate_->PopSentMessage());
  display_.AdjustBrightnessByPercent(-5.0);
  display_.AdjustBrightnessByPercent(5.0);
  EXPECT_TRUE(test_api_.TriggerTimeout());
  EXPECT_EQ("", delegate_->PopSentMessage());
}

}  // namespace system
}  // namespace power_manager
