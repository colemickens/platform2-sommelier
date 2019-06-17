// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_MOCK_JOURNAL_H_
#define MODEMFWD_MOCK_JOURNAL_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "modemfwd/journal.h"

namespace modemfwd {

class MockJournal : public Journal {
 public:
  MockJournal() {}
  ~MockJournal() override = default;

  MOCK_METHOD2(MarkStartOfFlashingMainFirmware,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(MarkEndOfFlashingMainFirmware,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(MarkStartOfFlashingCarrierFirmware,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(MarkEndOfFlashingCarrierFirmware,
               void(const std::string&, const std::string&));
};

}  // namespace modemfwd

#endif  // MODEMFWD_MOCK_JOURNAL_H_
