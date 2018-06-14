// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <memory>

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop/message_loop.h>

#include "hermes/esim_qmi_impl.h"
#include "hermes/lpd.h"
#include "hermes/smdp_fi_impl.h"

// TODO(jruthe): update this with some actual functionality to mimic caller.
void ErrorCallback(hermes::LpdError error) {}

void SuccessCallback() {}

int main(int argc, char** argv) {
  base::MessageLoop message_loop;
  hermes::Lpd lpd(std::make_unique<hermes::EsimQmiImpl>(),
                  std::make_unique<hermes::SmdpFiImpl>());
  lpd.InstallProfile(base::Bind(&SuccessCallback), base::Bind(&ErrorCallback));

  return EXIT_SUCCESS;
}
