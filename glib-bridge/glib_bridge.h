// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple event bridge to allow glib programs to operate on a libchrome
// message loop.

#ifndef GLIB_BRIDGE_GLIB_BRIDGE_H_
#define GLIB_BRIDGE_GLIB_BRIDGE_H_

#include <glib.h>

#include <map>
#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/cancelable_callback.h>
#include <base/message_loop/message_loop.h>

namespace glib_bridge {

struct GlibBridge : public base::MessageLoopForIO::Watcher {
 public:
  explicit GlibBridge(base::MessageLoopForIO* message_loop);

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanWriteWithoutBlocking(int fd);
  void OnFileCanReadWithoutBlocking(int fd);

 private:
  enum class State {
    kPreparingIteration,
    kWaitingForEvents,
    kReadyForDispatch,
  };

  void PrepareIteration();
  void Dispatch();

  void OnEvent(int fd, int flag);

  // If we ever need to support multiple GMainContexts instead of just the
  // default one then we can wrap a different context here. This is a weak
  // pointer.
  GMainContext* glib_context_;

  // glib event and source bits.
  int max_priority_ = -1;
  std::vector<GPollFD> poll_fds_;
  std::map<int, std::vector<GPollFD*>> fd_map_;

  // libchrome message loop bits.
  base::MessageLoopForIO* message_loop_;
  std::vector<std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher>>
      watchers_;
  base::CancelableClosure timeout_closure_;

  State state_;

  base::WeakPtrFactory<GlibBridge> weak_ptr_factory_;
};

}  // namespace glib_bridge

#endif  // GLIB_BRIDGE_GLIB_BRIDGE_H_
