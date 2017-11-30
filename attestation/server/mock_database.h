// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_SERVER_MOCK_DATABASE_H_
#define ATTESTATION_SERVER_MOCK_DATABASE_H_

#include "attestation/server/database.h"

#include <gmock/gmock.h>

namespace attestation {

class MockDatabase : public Database {
 public:
  MockDatabase();
  ~MockDatabase() override;

  MOCK_CONST_METHOD0(GetProtobuf, const AttestationDatabase&());
  MOCK_METHOD0(GetMutableProtobuf, AttestationDatabase*());
  MOCK_METHOD0(SaveChanges, bool());
  MOCK_METHOD0(Reload, bool());

 private:
  AttestationDatabase fake_;
};

}  // namespace attestation

#endif  // ATTESTATION_SERVER_MOCK_DATABASE_H_
