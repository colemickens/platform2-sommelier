// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef MIDIS_TESTS_SEQ_HANDLER_MOCK_H_
#define MIDIS_TESTS_SEQ_HANDLER_MOCK_H_

#include <gmock/gmock.h>

#include "midis/seq_handler.h"

namespace midis {

class SeqHandlerMock : public SeqHandler {
 public:
  MOCK_METHOD2(SndSeqEventOutputDirect,
               int(snd_seq_t* out_client, snd_seq_event_t* event));
  MOCK_METHOD2(SndSeqEventInput,
               int(snd_seq_t* in_client, snd_seq_event_t** event));
  MOCK_METHOD2(SndSeqEventInputPending,
               int(snd_seq_t* in_client, int fetch_sequencer));
  MOCK_METHOD1(AddSeqDevice, void(uint32_t device_id));
  MOCK_METHOD2(AddSeqPort, void(uint32_t device_id, uint32_t port_id));
  MOCK_METHOD1(RemoveSeqDevice, void(uint32_t device_id));
  MOCK_METHOD2(RemoveSeqPort, void(uint32_t device_id, uint32_t port_id));
  MOCK_METHOD1(ProcessMidiEvent, void(snd_seq_event_t* event));
};

}  // namespace midis
#endif  //  MIDIS_TESTS_SEQ_HANDLER_MOCK_H_
