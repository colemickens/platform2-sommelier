// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_EXTERNAL_TASK_H_
#define SHILL_MOCK_EXTERNAL_TASK_H_

#include <map>
#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/external_task.h"

namespace shill {

class MockExternalTask : public ExternalTask {
 public:
  MockExternalTask(ControlInterface* control,
                   ProcessManager* process_manager,
                   const base::WeakPtr<RpcTaskDelegate>& task_delegate,
                   const base::Callback<void(pid_t, int)>& death_callback);
  ~MockExternalTask() override;

  MOCK_METHOD5(Start,
               bool(const base::FilePath& file,
                    const std::vector<std::string>& arguments,
                    const std::map<std::string, std::string>& environment,
                    bool terminate_with_parent,
                    Error* error));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(OnDelete, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockExternalTask);
};

}  // namespace shill

#endif  // SHILL_MOCK_EXTERNAL_TASK_H_
