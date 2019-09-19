// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_FP_CONTEXT_COMMAND_H_
#define BIOD_FP_CONTEXT_COMMAND_H_

#include <memory>
#include <string>

#include "biod/ec_command.h"
#include "biod/ec_command_async.h"

namespace biod {

class FpContextCommand_v0
    : public EcCommand<struct ec_params_fp_context, EmptyParam> {
 public:
  ~FpContextCommand_v0() override = default;
  static std::unique_ptr<FpContextCommand_v0> Create(const std::string& hex);

 private:
  FpContextCommand_v0();
};

class FpContextCommand_v1
    : public EcCommandAsync<struct ec_params_fp_context_v1, EmptyParam> {
 public:
  ~FpContextCommand_v1() override = default;
  static std::unique_ptr<FpContextCommand_v1> Create(const std::string& hex);

 private:
  using Options =
      EcCommandAsync<struct ec_params_fp_context_v1, EmptyParam>::Options;
  FpContextCommand_v1();
};

}  // namespace biod

#endif  // BIOD_FP_CONTEXT_COMMAND_H_
