// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <memory>

#include <base/bind.h>
#include <base/callback.h>

#include "hermes/esim_uim_impl.h"
#include "hermes/lpd.h"
#include "hermes/smdp_fi_impl.h"

void ErrorCallback(const std::vector<uint8_t>& data) {}

void SuccessCallback(const std::vector<uint8_t>& data) {}

int main(int argc, char** argv) {
  hermes::Lpd lpd(std::make_unique<hermes::EsimUimImpl>(),
                  std::make_unique<hermes::SmdpFiImpl>());
  lpd.InstallProfile(base::Bind(&SuccessCallback), base::Bind(&ErrorCallback));

  return EXIT_SUCCESS;
}
