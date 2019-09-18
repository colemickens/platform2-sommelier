// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_EC_COMMAND_ASYNC_H_
#define BIOD_EC_COMMAND_ASYNC_H_

#include <base/logging.h>
#include <base/threading/thread.h>
#include <base/time/time.h>

#include "biod/ec_command.h"

namespace biod {

/**
 * Represents an "async" EC command. Note that the EC codebase does not
 * support true asynchronous commands. All commands are expected to return
 * within a certain deadline (currently 200 ms). To handle longer-running
 * commands, the EC codebase has adopted a style where a command is first
 * started and then the result is polled for by specifying an |action| in the
 * command's request parameters. See EC_CMD_FLASH_ERASE and EC_CMD_ADD_ENTROPY
 * for examples.
 */
template <typename O, typename I>
class EcCommandAsync : public EcCommand<O, I> {
 public:
  EcCommandAsync(uint32_t cmd,
                 uint8_t async_result_action,
                 uint32_t ver = 0,
                 const O& req = {})
      : EcCommand<O, I>(cmd, ver, req),
        async_result_action_(async_result_action) {}

  struct Options {
    int poll_for_result_num_attempts = 20;
    base::TimeDelta poll_interval = base::TimeDelta::FromMilliseconds(100);
    /**
     * When polling for the result, the EC should normally return EC_RES_BUSY
     * when the command is still being processed. However, some commands
     * cause the EC to temporarily stop responding to EC commands and the ioctl
     * times out. Those commands should set validate_poll_result to false to
     * ignore that error and continue polling until the timeout is hit.
     */
    bool validate_poll_result = true;
  };

  bool Run(int fd, const Options& options) {
    CHECK_GT(options.poll_for_result_num_attempts, 0);

    if (!BaseCmd::Run(fd)) {
      LOG(ERROR) << "Failed to start command";
      return false;
    }

    int num_attempts = options.poll_for_result_num_attempts;
    while (num_attempts--) {
      base::PlatformThread::Sleep(options.poll_interval);

      BaseCmd::Req()->action = async_result_action_;
      BaseCmd::Run(fd);
      auto ret = BaseCmd::Result();
      if (ret == EC_RES_SUCCESS) {
        return true;
      }

      if (options.validate_poll_result && ret != EC_RES_BUSY) {
        LOG(ERROR) << "Failed to get command result, ret: " << ret;
        return false;
      }
    }

    LOG(ERROR) << "Timed out polling for command 0x" << std::hex
               << BaseCmd::data_.cmd.command;
    return false;
  }

 private:
  using BaseCmd = EcCommand<O, I>;
  uint8_t async_result_action_;
};

}  // namespace biod

#endif  // BIOD_EC_COMMAND_ASYNC_H_
