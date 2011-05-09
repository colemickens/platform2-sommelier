// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MountTask - The basis for asynchronous API work items.  It inherits from
// Task, which allows it to be called on an event thread.  Subclasses define the
// specific asychronous request, such as MountTaskMount, MountTaskMountGuest,
// MountTaskMigratePasskey, MountTaskUnmount, and MountTaskTestCredentials.
// Asynchronous tasks in cryptohome are serialized calls on a single worker
// thread separate from the dbus main event loop.  The synchronous versions of
// these APIs are also done on this worker thread, with the main thread waiting
// on a completion event to return.  This allows all of these calls to be
// serialized, as we use a common mount point for cryptohome.
//
// Also defined here is the MountTaskResult, which has the task result
// information, and the MountTaskObserver, which is called when a task is
// completed.
//
// Notifications can happen either by setting the completion event or providing
// a MountTaskObserver.  The former is used in Service (see service.cc) when
// faking synchronous versions of these tasks, and the latter is used in the
// asychronous versions.

#ifndef CRYPTOHOME_MOUNT_TASK_H_
#define CRYPTOHOME_MOUNT_TASK_H_

#include <base/atomic_sequence_num.h>
#include <base/thread.h>
#include <base/waitable_event.h>
#include <chromeos/process.h>
#include <chromeos/utility.h>

#include "cryptohome_event_source.h"
#include "mount.h"
#include "pkcs11_init.h"
#include "username_passkey.h"

namespace cryptohome {

extern const char* kMountTaskResultEventType;
extern const char* kPkcs11InitResultEventType;

class MountTaskResult : public CryptohomeEventBase {
 public:
  MountTaskResult()
      : sequence_id_(-1),
        return_status_(false),
        return_code_(Mount::MOUNT_ERROR_NONE),
        event_name_(kMountTaskResultEventType) { }

  // Constructor which sets an alternative event name. Useful
  // for using MountTaskResult for other event types.
  MountTaskResult(const char* event_name)
      : sequence_id_(-1),
        return_status_(false),
        return_code_(Mount::MOUNT_ERROR_NONE),
        event_name_(event_name) { }

  // Copy constructor is necessary for inserting MountTaskResult into the events
  // vector in CryptohomeEventSource.
  MountTaskResult(const MountTaskResult& rhs)
      : sequence_id_(rhs.sequence_id_),
        return_status_(rhs.return_status_),
        return_code_(rhs.return_code_),
        event_name_(rhs.event_name_) { }

  virtual ~MountTaskResult() { }

  // Get the asynchronous task id
  int sequence_id() const {
    return sequence_id_;
  }

  // Set the asynchronous task id
  void set_sequence_id(int value) {
    sequence_id_ = value;
  }

  // Get the status of the call
  bool return_status() const {
    return return_status_;
  }

  // Set the status of the call
  void set_return_status(bool value) {
    return_status_ = value;
  }

  // Get the MountError for applicable calls (Mount, MountGuest)
  Mount::MountError return_code() const {
    return return_code_;
  }

  // Set the MountError for applicable calls (Mount, MountGuest)
  void set_return_code(Mount::MountError value) {
    return_code_ = value;
  }

  MountTaskResult& operator=(const MountTaskResult& rhs) {
    sequence_id_ = rhs.sequence_id_;
    return_status_ = rhs.return_status_;
    return_code_ = rhs.return_code_;
    event_name_ = rhs.event_name_;
    return *this;
  }

  virtual const char* GetEventName() {
    return event_name_;
  }

 private:
  int sequence_id_;
  bool return_status_;
  Mount::MountError return_code_;
  const char* event_name_;
};

class MountTaskObserver {
 public:
  MountTaskObserver() {}
  virtual ~MountTaskObserver() {}

  // Called by the MountTask when the task is complete
  virtual void MountTaskObserve(const MountTaskResult& result) = 0;
};

class MountTask : public Task {
 public:
  MountTask(MountTaskObserver* observer,
            Mount* mount,
            const UsernamePasskey& credentials);
  virtual ~MountTask();

  // Run is called by the worker thread when this task is being processed
  virtual void Run() {
    Notify();
  }

  // Gets the asynchronous call id of this task
  int sequence_id() {
    return sequence_id_;
  }

  // Gets the MountTaskResult for this task
  MountTaskResult* result() {
    return result_;
  }

  // Sets the MountTaskResult for this task
  void set_result(MountTaskResult* result) {
    result_ = result;
    result_->set_sequence_id(sequence_id_);
  }

  // Sets the event to be signaled when the event is completed
  void set_complete_event(base::WaitableEvent* value) {
    complete_event_ = value;
  }

 protected:
  // Notify implements the default behavior when this task is complete
  void Notify();

  // The Mount instance that does the actual work
  Mount* mount_;

  // The Credentials associated with this task
  UsernamePasskey credentials_;

  // The asychronous call id for this task
  int sequence_id_;

 private:
  // Signal will call Signal on the completion event if it is set
  void Signal();

  // Return the next sequence number
  static int NextSequence();

  // The MountTaskObserver to be notified when this task is complete
  MountTaskObserver* observer_;

  // The default instance of MountTaskResult to use if it is not set by the
  // caller
  scoped_ptr<MountTaskResult> default_result_;

  // The actual instance of MountTaskResult to use
  MountTaskResult* result_;

  // The completion event to signal when this task is complete
  base::WaitableEvent* complete_event_;

  // An atomic incrementing sequence for setting asynchronous call ids
  static base::AtomicSequenceNumber sequence_holder_;

  DISALLOW_COPY_AND_ASSIGN(MountTask);
};

// Implements a no-op task that merely posts results
class MountTaskNop : public MountTask {
 public:
  MountTaskNop(MountTaskObserver* observer)
      : MountTask(observer, NULL, UsernamePasskey()) {
  }
  virtual ~MountTaskNop() { }

  virtual void Run() {
    Notify();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskNop);
};

// Implements asychronous calls to Mount::Mount()
class MountTaskMount : public MountTask {
 public:
  MountTaskMount(MountTaskObserver* observer,
                 Mount* mount,
                 const UsernamePasskey& credentials,
                 const Mount::MountArgs& mount_args)
      : MountTask(observer, mount, credentials) {
    mount_args_.CopyFrom(mount_args);
  }
  virtual ~MountTaskMount() { }

  virtual void Run();

 private:
  Mount::MountArgs mount_args_;

  DISALLOW_COPY_AND_ASSIGN(MountTaskMount);
};

// Implements asychronous calls to Mount::MountGuest()
class MountTaskMountGuest : public MountTask {
 public:
  MountTaskMountGuest(MountTaskObserver* observer,
                      Mount* mount)
      : MountTask(observer, mount, UsernamePasskey()) {
  }
  virtual ~MountTaskMountGuest() { }

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskMountGuest);
};

// Implements asychronous calls to Mount::MigratePasskey()
class MountTaskMigratePasskey : public MountTask {
 public:
  MountTaskMigratePasskey(MountTaskObserver* observer,
            Mount* mount,
            const UsernamePasskey& credentials,
            const char* old_key)
      : MountTask(observer, mount, credentials) {
    old_key_.resize(strlen(old_key) + 1);
    memcpy(old_key_.data(), old_key, old_key_.size());
    old_key_[old_key_.size() - 1] = 0;
  }
  virtual ~MountTaskMigratePasskey() { }

  virtual void Run();

 private:
  SecureBlob old_key_;

  DISALLOW_COPY_AND_ASSIGN(MountTaskMigratePasskey);
};

// Implements asychronous calls to Mount::Unmount()
class MountTaskUnmount : public MountTask {
 public:
  MountTaskUnmount(MountTaskObserver* observer,
                   Mount* mount)
      : MountTask(observer, mount, UsernamePasskey()) {
  }
  virtual ~MountTaskUnmount() { }

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskUnmount);
};

// Implements asychronous calls to Mount::TestCredentials()
class MountTaskTestCredentials : public MountTask {
 public:
  MountTaskTestCredentials(MountTaskObserver* observer,
                           Mount* mount,
                           const UsernamePasskey& credentials)
      : MountTask(observer, mount, credentials) {
  }
  virtual ~MountTaskTestCredentials() { }

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskTestCredentials);
};

// Implements asychronous calls to Mount::RemoveCryptohome()
class MountTaskRemove : public MountTask {
 public:
  MountTaskRemove(MountTaskObserver* observer,
                  Mount* mount,
                  const UsernamePasskey& credentials)
      : MountTask(observer, mount, credentials) {
  }
  virtual ~MountTaskRemove() { }

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskRemove);
};

// Implements asychronous reset of the TPM context
class MountTaskResetTpmContext : public MountTask {
 public:
  MountTaskResetTpmContext(MountTaskObserver* observer, Mount* mount)
      : MountTask(observer, mount, UsernamePasskey()) {
  }
  virtual ~MountTaskResetTpmContext() { }

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskResetTpmContext);
};

// Implements asychronous removal of tracked subdirectories
class MountTaskRemoveTrackedSubdirectories : public MountTask {
 public:
  MountTaskRemoveTrackedSubdirectories(MountTaskObserver* observer,
                                       Mount* mount)
      : MountTask(observer, mount, UsernamePasskey()) {
  }
  virtual ~MountTaskRemoveTrackedSubdirectories() { }

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskRemoveTrackedSubdirectories);
};

// Implements asychronous removal of tracked subdirectories
class MountTaskAutomaticFreeDiskSpace : public MountTask {
 public:
  MountTaskAutomaticFreeDiskSpace(MountTaskObserver* observer,
                                  Mount* mount)
      : MountTask(observer, mount, UsernamePasskey()) {
  }
  virtual ~MountTaskAutomaticFreeDiskSpace() { }

  virtual void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskAutomaticFreeDiskSpace);
};

// Implements asychronous setting owner user
class MountTaskSetOwnerUser : public MountTask {
 public:
  MountTaskSetOwnerUser(MountTaskObserver* observer,
                        Mount* mount,
                        const char* username)
      : MountTask(observer, mount, UsernamePasskey()),
        username_(username) {
  }
  virtual ~MountTaskSetOwnerUser() { }

  virtual void Run();

 private:
  const std::string username_;
  DISALLOW_COPY_AND_ASSIGN(MountTaskSetOwnerUser);
};

// Implements asynchronous updating of the current user (if any)
// activity timestamp
class MountTaskUpdateCurrentUserActivityTimestamp : public MountTask {
 public:
  MountTaskUpdateCurrentUserActivityTimestamp(MountTaskObserver* observer,
                                              Mount* mount,
                                              int time_shift_sec)
      : MountTask(observer, mount, UsernamePasskey()),
        time_shift_sec_(time_shift_sec) {
  }
  virtual ~MountTaskUpdateCurrentUserActivityTimestamp() { }

  virtual void Run();

 private:
  const int time_shift_sec_;
  DISALLOW_COPY_AND_ASSIGN(MountTaskUpdateCurrentUserActivityTimestamp);
};

// Implements asynchronous initialization of Pkcs11.
class MountTaskPkcs11Init : public MountTask {
 public:
  static const std::string kPkcs11InitCmd[];

  MountTaskPkcs11Init(MountTaskObserver* observer, Mount* mount);
  virtual ~MountTaskPkcs11Init() { }

  virtual void Run();

 private:
  scoped_ptr<MountTaskResult> pkcs11_init_result_;
  scoped_ptr<chromeos::ProcessImpl> default_pkcs11_initializer_;
  chromeos::ProcessImpl* pkcs11_initializer_;
  DISALLOW_COPY_AND_ASSIGN(MountTaskPkcs11Init);
};

}  // namespace cryptohome

#endif // CRYPTOHOME_MOUNT_TASK_H_
