/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBCHROMEOS_CHROMEOS_BINDER_WATCHER_H_
#define LIBCHROMEOS_CHROMEOS_BINDER_WATCHER_H_

#include <base/macros.h>
#include <base/message_loop/message_loop.h>

namespace chromeos {

// Bridge between libbinder and base::MessageLoop. Construct at startup to make
// the message loop watch for binder events and pass them to libbinder.
class BinderWatcher : public base::MessageLoopForIO::Watcher {
 public:
  BinderWatcher();
  ~BinderWatcher() override;

  // Initializes the object, returning true on success.
  bool Init();

  // base::MessageLoopForIO::Watcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(BinderWatcher);
};

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_BINDER_WATCHER_H_
