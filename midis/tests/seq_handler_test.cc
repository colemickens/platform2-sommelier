// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include <brillo/test_helpers.h>
#include <gtest/gtest.h>

#include "midis/tests/seq_handler_mock.h"
#include "midis/tests/test_helper.h"

using ::testing::_;
using ::testing::Return;

namespace {

const std::array<uint8_t, 3> kValidBuffer1 = {{0x90, 0x3C, 0x40}};
const std::array<uint8_t, 3> kValidBuffer2 = {{0xC0, 0x0B}};
const std::array<uint8_t, 4> kInvalidBuffer3 = {{0x0A, 0x0B, 0x0C, 0x0D}};

const int kCorrectOutputDirectReturn = 28;

}  //  namespace

namespace midis {

class SeqHandlerTest : public ::testing::Test {};

// Check whether Device gets created successfully.
TEST_F(SeqHandlerTest, TestEncodeBytes) {
  auto seq_handler = std::make_unique<SeqHandlerMock>();

  EXPECT_CALL(*seq_handler, SndSeqEventOutputDirect(_, _))
      .WillOnce(Return(kCorrectOutputDirectReturn))
      .WillOnce(Return(kCorrectOutputDirectReturn))
      .WillOnce(Return(kCorrectOutputDirectReturn + 1));

  snd_midi_event_t* encoder;

  // Test that encoding works correctly.
  ASSERT_EQ(snd_midi_event_new(kValidBuffer1.size(), &encoder), 0);
  EXPECT_EQ(seq_handler->EncodeMidiBytes(0, nullptr, kValidBuffer1.data(),
                                         kValidBuffer1.size(), encoder),
            true);
  snd_midi_event_free(encoder);

  // Test that encoding works correctly - 2.
  ASSERT_EQ(snd_midi_event_new(kValidBuffer2.size(), &encoder), 0);
  EXPECT_EQ(seq_handler->EncodeMidiBytes(0, nullptr, kValidBuffer2.data(),
                                         kValidBuffer2.size(), encoder),
            true);
  snd_midi_event_free(encoder);

  // Test for failure when OutputDirect returns incorrect value.
  ASSERT_EQ(snd_midi_event_new(kValidBuffer1.size(), &encoder), 0);
  EXPECT_EQ(seq_handler->EncodeMidiBytes(0, nullptr, kValidBuffer1.data(),
                                         kValidBuffer1.size(), encoder),
            false);
  snd_midi_event_free(encoder);

  // Test for failure when we supply gibberish data.
  ASSERT_EQ(snd_midi_event_new(kInvalidBuffer3.size(), &encoder), 0);
  EXPECT_EQ(seq_handler->EncodeMidiBytes(0, nullptr, kInvalidBuffer3.data(),
                                         kInvalidBuffer3.size(), encoder),
            false);
  snd_midi_event_free(encoder);
}

}  // namespace midis
