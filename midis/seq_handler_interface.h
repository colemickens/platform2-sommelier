// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_SEQ_HANDLER_INTERFACE_H_
#define MIDIS_SEQ_HANDLER_INTERFACE_H_

#include <stdint.h>

#include <alsa/asoundlib.h>

namespace midis {

class SeqHandlerInterface {
 public:
  virtual ~SeqHandlerInterface() = default;

  virtual bool InitSeq() = 0;

  virtual void ProcessAlsaClientFd() = 0;

  virtual void AddSeqDevice(uint32_t device_id) = 0;

  virtual void AddSeqPort(uint32_t device_id, uint32_t port_id) = 0;

  virtual void RemoveSeqDevice(uint32_t device_id) = 0;

  virtual void RemoveSeqPort(uint32_t device_id, uint32_t port_id) = 0;

  virtual bool SubscribeInPort(uint32_t device_id, uint32_t port_id) = 0;

  virtual int SubscribeOutPort(uint32_t device_id, uint32_t port_id) = 0;

  virtual void UnsubscribeInPort(uint32_t device_id, uint32_t port_id) = 0;

  virtual void UnsubscribeOutPort(int out_port_id) = 0;

  virtual void SendMidiData(int out_port_id,
                            const uint8_t* buffer,
                            size_t buf_len) = 0;

  virtual void ProcessMidiEvent(snd_seq_event_t* event) = 0;
};

}  // namespace midis

#endif  // MIDIS_SEQ_HANDLER_INTERFACE_H_
