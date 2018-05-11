// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_H_

#include <memory>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/single_thread_task_runner.h>

#include "bluetooth/newblued/libnewblue.h"

namespace bluetooth {

// A higher-level API wrapper of the low-level libnewblue C interface.
// This class abstracts away the C interface details of libnewblue and provides
// event handling model that is compatible with libchrome's main loop.
class Newblue {
 public:
  explicit Newblue(std::unique_ptr<LibNewblue> libnewblue);
  virtual ~Newblue() = default;

  base::WeakPtr<Newblue> GetWeakPtr();

  // Initializes the LE stack (blocking call).
  // Returns true if initialization succeeds, false otherwise.
  virtual bool Init();

  // Listens to reset complete event from the chip. This is useful to detect
  // when NewBlue is ready to bring up the stack.
  virtual void ListenReadyForUp(base::Closure callback);

  // Brings up the NewBlue stack. This should be called when the adapter has
  // just been turned on, detected when there is reset complete event from the
  // chip. ListenReadyForUp() should be used to detect this event.
  // Returns true if success, false otherwise.
  virtual bool BringUp();

 private:
  // Posts task to the thread which created this Newblue object.
  // libnewblue callbacks should always post task using this method rather than
  // doing any processing in the callback's thread.
  bool PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task);

  // Called by NewBlue when it's ready to bring up the stack. This is called on
  // one of NewBlue's threads, so we shouldn't do anything on this thread other
  // than posting the task to our mainloop thread.
  static void OnStackReadyForUpThunk(void* data);
  // Triggers the callback registered via ListenReadyForUp().
  void OnStackReadyForUp();

  std::unique_ptr<LibNewblue> libnewblue_;

  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  base::Closure ready_for_up_callback_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Newblue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Newblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_H_
