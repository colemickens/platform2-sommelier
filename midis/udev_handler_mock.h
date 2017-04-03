// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef MIDIS_UDEV_HANDLER_MOCK_H_
#define MIDIS_UDEV_HANDLER_MOCK_H_

#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "midis/device_tracker.h"

namespace midis {

class UdevHandlerMock : public UdevHandler {
 public:
  UdevHandlerMock() : UdevHandler(nullptr) {}
  MOCK_METHOD1(GetMidiDeviceDnameMock,
               std::string(struct udev_device* udev_device));
  MOCK_METHOD1(GetDeviceInfoMock, snd_rawmidi_info*(const std::string& name));

  std::string GetMidiDeviceDname(struct udev_device* udev_device) override {
    return GetMidiDeviceDnameMock(udev_device);
  }

  std::unique_ptr<snd_rawmidi_info> GetDeviceInfo(
      const std::string& name) override {
    return std::unique_ptr<snd_rawmidi_info>(GetDeviceInfoMock(name));
  }
};

}  // namespace midis

#endif  //  MIDIS_UDEV_HANDLER_MOCK_H_
