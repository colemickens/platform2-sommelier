// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_MOCK_USB_UTILS_H_
#define HAMMERD_MOCK_USB_UTILS_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "hammerd/usb_utils.h"

namespace hammerd {

ACTION_P(WriteBuf, ptr) {
  memcpy(arg0, ptr, arg1);
  return arg1;
}

class MockUsbEndpoint : public UsbEndpoint {
 public:
  MockUsbEndpoint() = default;
  ~MockUsbEndpoint() override = default;
  MOCK_METHOD0(Connect, bool());
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(GetChunkLength, size_t());

  int Send(const void* outbuf, int outlen, unsigned int timeout) override {
    // We only care about the value of the output buffer.
    auto out_ptr = reinterpret_cast<const uint8_t*>(outbuf);
    std::vector<uint8_t> out(out_ptr, out_ptr + outlen);
    return SendHelper(out, outbuf, outlen);
  }

  MOCK_METHOD3(SendHelper,
               int(std::vector<uint8_t> out, const void* outbuf, int outlen));

  MOCK_METHOD4(
      Receive,
      int(void* inbuf, int inlen, bool allow_less, unsigned int timeout));
};

}  // namespace hammerd
#endif  // HAMMERD_MOCK_USB_UTILS_H_
