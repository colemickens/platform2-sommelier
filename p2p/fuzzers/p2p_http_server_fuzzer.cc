// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "p2p/common/server_message.h"
#include "p2p/server/http_server_external_process.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/test/fuzzed_data_provider.h"

#include <metrics/metrics_library.h>

using p2p::server::HttpServerExternalProcess;

using p2p::util::kNumP2PServerMessageTypes;
using p2p::util::kP2PServerMagic;
using p2p::util::P2PServerMessage;

using base::FilePath;

struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);  // Disable logging.
  }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  if (size < 1)
    return 0;

  base::FuzzedDataProvider data_provider(data, size);

  // The values of magic and message_type are constrained to ensure
  // OnMessageReceived does not exit().
  P2PServerMessage msg = (P2PServerMessage){
      .magic = kP2PServerMagic,
      .message_type =
          data_provider.ConsumeUint32InRange(0, kNumP2PServerMessageTypes - 1),
      .value = (static_cast<int64_t>(data_provider.ConsumeInt32InRange(
                    std::numeric_limits<int32_t>::min(),
                    std::numeric_limits<int32_t>::max()))
                << 32) |
               (static_cast<int64_t>(data_provider.ConsumeInt32InRange(
                   std::numeric_limits<int32_t>::min(),
                   std::numeric_limits<int32_t>::max())))};

  // Create HTTP server external process.
  MetricsLibrary metrics_lib;
  metrics_lib.Init();
  HttpServerExternalProcess* process = new HttpServerExternalProcess(
      &metrics_lib, FilePath("/tmp/p2p-fuzzing.XXXXXX"), FilePath("."), 0);

  // There's no need to Start() the process since OnMessageReceived only updates
  // member variables or sends metrics using the provided metrics library.
  HttpServerExternalProcess::OnMessageReceived(msg, process);

  delete process;

  return 0;
}
