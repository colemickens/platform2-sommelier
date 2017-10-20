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
using ::testing::SetArgPointee;

namespace {

const std::array<uint8_t, 3> kValidBuffer1 = {{0x90, 0x3C, 0x40}};
const std::array<uint8_t, 3> kValidBuffer2 = {{0xC0, 0x0B}};
const std::array<uint8_t, 4> kInvalidBuffer3 = {{0x0A, 0x0B, 0x0C, 0x0D}};

const int kCorrectOutputDirectReturn = 28;
const int kOutClientId = 2;

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

// Check that ProcessAlsaClientFd errors out correctly for various error inputs.
TEST_F(SeqHandlerTest, TestProcessAlsaClientFdNegative) {
  auto seq_handler = std::make_unique<SeqHandlerMock>();

  // None of these functions should ever be called.
  EXPECT_CALL(*seq_handler, AddSeqDevice(_)).Times(0);
  EXPECT_CALL(*seq_handler, AddSeqPort(_, _)).Times(0);
  EXPECT_CALL(*seq_handler, RemoveSeqDevice(_)).Times(0);
  EXPECT_CALL(*seq_handler, RemoveSeqPort(_, _)).Times(0);
  EXPECT_CALL(*seq_handler, ProcessMidiEvent(_)).Times(0);

  EXPECT_CALL(*seq_handler, SndSeqEventInput(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(nullptr), Return(-ENOSPC)));
  EXPECT_CALL(*seq_handler, SndSeqEventInputPending(_, _)).WillOnce(Return(0));

  seq_handler->ProcessAlsaClientFd();

  const snd_seq_event_t kInvalidEvent1 = {
    .source = {
      .client = SND_SEQ_CLIENT_SYSTEM,
      .port = SND_SEQ_PORT_SYSTEM_ANNOUNCE,
    },
    // This event type should never show up on this client+port.
    .type = SND_SEQ_EVENT_SONGPOS,
  };

  // Check invalid events.
  EXPECT_CALL(*seq_handler, SndSeqEventInput(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(const_cast<snd_seq_event_t*>(&kInvalidEvent1)),
                Return(0)));
  EXPECT_CALL(*seq_handler, SndSeqEventInputPending(_, _)).WillOnce(Return(0));

  seq_handler->ProcessAlsaClientFd();
}

// Check that ProcessAlsaClientFd handles various valid events correctly.
TEST_F(SeqHandlerTest, TestProcessAlsaClientFdPositive) {
  auto seq_handler = std::make_unique<SeqHandlerMock>();

  const snd_seq_event_t kValidEvent2 = {
    .source = {
      .client = SND_SEQ_CLIENT_SYSTEM,
      .port = SND_SEQ_PORT_SYSTEM_ANNOUNCE,
    },
    .type = SND_SEQ_EVENT_PORT_START,
  };

  EXPECT_CALL(*seq_handler, AddSeqDevice(_)).Times(1);
  EXPECT_CALL(*seq_handler, AddSeqPort(_, _)).Times(1);
  EXPECT_CALL(*seq_handler, RemoveSeqDevice(_)).Times(0);
  EXPECT_CALL(*seq_handler, RemoveSeqPort(_, _)).Times(0);
  EXPECT_CALL(*seq_handler, ProcessMidiEvent(_)).Times(0);
  EXPECT_CALL(*seq_handler, SndSeqEventInput(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(const_cast<snd_seq_event_t*>(&kValidEvent2)),
                Return(0)));
  EXPECT_CALL(*seq_handler, SndSeqEventInputPending(_, _)).WillOnce(Return(0));

  seq_handler->ProcessAlsaClientFd();

  const snd_seq_event_t kValidEvent3 = {
    .source = {
      .client = SND_SEQ_CLIENT_SYSTEM,
      .port = SND_SEQ_PORT_SYSTEM_ANNOUNCE,
    },
    .type = SND_SEQ_EVENT_CLIENT_EXIT,
    .data = {
      .addr = {
        .client = 3,
        .port = 4,
      }
    },
  };

  seq_handler->out_client_id_ = kOutClientId;
  EXPECT_CALL(*seq_handler, AddSeqDevice(_)).Times(0);
  EXPECT_CALL(*seq_handler, AddSeqPort(_, _)).Times(0);
  EXPECT_CALL(*seq_handler, RemoveSeqDevice(_)).Times(1);
  EXPECT_CALL(*seq_handler, RemoveSeqPort(_, _)).Times(0);
  EXPECT_CALL(*seq_handler, ProcessMidiEvent(_)).Times(0);
  EXPECT_CALL(*seq_handler, SndSeqEventInput(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(const_cast<snd_seq_event_t*>(&kValidEvent3)),
                Return(0)));
  EXPECT_CALL(*seq_handler, SndSeqEventInputPending(_, _)).WillOnce(Return(0));

  seq_handler->ProcessAlsaClientFd();
}

}  // namespace midis
