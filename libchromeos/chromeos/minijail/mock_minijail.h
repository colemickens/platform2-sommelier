// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_MINIJAIL_MOCK_MINIJAIL_H_
#define LIBCHROMEOS_CHROMEOS_MINIJAIL_MOCK_MINIJAIL_H_

#include <vector>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "libchromeos/chromeos/minijail/minijail.h"

namespace chromeos {

class MockMinijail : public chromeos::Minijail {
 public:
  MockMinijail() {}
  virtual ~MockMinijail() {}

  MOCK_METHOD0(New, struct minijail *());
  MOCK_METHOD1(Destroy, void(struct minijail *));

  MOCK_METHOD2(DropRoot, bool(struct minijail *jail, const char *user));
  MOCK_METHOD2(UseCapabilities, void(struct minijail *jail, uint64_t capmask));
  MOCK_METHOD3(Run, bool(struct minijail *jail,
                         std::vector<char *> args, pid_t *pid));
  MOCK_METHOD3(RunSync, bool(struct minijail *jail,
                             std::vector<char *> args, int *status));
  MOCK_METHOD3(RunAndDestroy, bool(struct minijail *jail,
                                   std::vector<char *> args, pid_t *pid));
  MOCK_METHOD3(RunSyncAndDestroy, bool(struct minijail *jail,
                                       std::vector<char *> args, int *status));
  MOCK_METHOD4(RunPipeAndDestroy, bool(struct minijail *jail,
                                       std::vector<char *> args,
                                       pid_t *pid, int *stdin));
  MOCK_METHOD6(RunPipesAndDestroy, bool(struct minijail *jail,
                                        std::vector<char *> args,
                                        pid_t *pid, int *stdin,
                                        int *stdout, int *stderr));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMinijail);
};

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_MINIJAIL_MOCK_MINIJAIL_H_
