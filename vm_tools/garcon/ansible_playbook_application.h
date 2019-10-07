// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_GARCON_ANSIBLE_PLAYBOOK_APPLICATION_H_
#define VM_TOOLS_GARCON_ANSIBLE_PLAYBOOK_APPLICATION_H_

#include <string>

namespace base {
class FilePath;
class WaitableEvent;
}  // namespace base

namespace vm_tools {
namespace garcon {

class AnsiblePlaybookApplicationObserver {
 public:
  virtual void OnApplyAnsiblePlaybookCompletion(
      bool success, const std::string& failure_reason) = 0;
};

// |event| is triggered once ansible-playbook is successfully spawned.
void ExecuteAnsiblePlaybook(AnsiblePlaybookApplicationObserver* observer,
                            base::WaitableEvent* event,
                            const base::FilePath& ansible_playbook_file_path,
                            std::string* error_msg);

base::FilePath CreateAnsiblePlaybookFile(const std::string& playbook,
                                         std::string* error_msg);

}  // namespace garcon
}  // namespace vm_tools

#endif  // VM_TOOLS_GARCON_ANSIBLE_PLAYBOOK_APPLICATION_H_
