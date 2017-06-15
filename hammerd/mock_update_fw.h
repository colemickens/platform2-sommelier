// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HAMMERD_MOCK_UPDATE_FW_H_
#define HAMMERD_MOCK_UPDATE_FW_H_

#include <string>

#include <gmock/gmock.h>

#include "hammerd/update_fw.h"

namespace hammerd {

class MockFirmwareUpdater : public FirmwareUpdaterInterface {
 public:
  MockFirmwareUpdater() = default;

  MOCK_METHOD1(LoadImage, bool(const std::string& image));
  MOCK_METHOD0(TryConnectUSB, bool());
  MOCK_METHOD0(CloseUSB, void());
  MOCK_METHOD0(SendFirstPDU, bool());
  MOCK_METHOD0(SendDone, void());
  MOCK_METHOD0(InjectEntropy, bool());
  MOCK_METHOD2(SendSubcommand,
               bool(UpdateExtraCommand subcommand,
                    const std::string& cmd_body));
  MOCK_METHOD1(TransferImage, bool(SectionName section_name));
  MOCK_CONST_METHOD0(CurrentSection, SectionName());
  MOCK_CONST_METHOD1(NeedsUpdate, bool(SectionName section_name));
  MOCK_CONST_METHOD1(IsSectionLocked, bool(SectionName section_name));
  MOCK_METHOD1(UnLockSection, bool(SectionName section_name));
};

}  // namespace hammerd
#endif  // HAMMERD_MOCK_UPDATE_FW_H_
