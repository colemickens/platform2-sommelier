// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _VPN_MANAGER_SERVICE_MANAGER_H_
#define _VPN_MANAGER_SERVICE_MANAGER_H_

#include <string>

#include "base/file_path.h"
#include "gtest/gtest_prod.h"  // for FRIEND_TEST

class ScopedTempDir;

// Generic code to manage setting up and stopping a set of layered
// tunnel services.  This object contains the code to manage a single
// layer.  Services are meant to be started from outermost to innermost.
// Services are meant to be stopped from the innermost out.  To
// stop the entire set of services, call Stop on the innermost.
// Services go from not-yet-started to started to in_running to
// was_stopped.
class ServiceManager {
 public:
  ServiceManager(const std::string& service_name);
  virtual ~ServiceManager();

  // Initialize directories used by services.  |scoped_temp_dir| will
  // be set to manage an appropriate temp directory.  This function
  // uses a reference to |scoped_temp_dir| and so its lifetime must be
  // equal to that of all objects derived from ServiceManager.
  static void InitializeDirectories(ScopedTempDir* scoped_temp_path);

  // Call to initiate this service.  If starting fails immediately this
  // returns false.  If something fails after this returns, OnStopped
  // will be called.  Code outside of the service manager stack
  // must only call Start on the outermost function.
  virtual bool Start() = 0;

  // Callback when this service has successfully started.
  virtual void OnStarted();

  // Call to stop this service.  Must not be called on a separate
  // thread from Start().  Code outside of the service manager stack
  // must only call Stop on the innermost service.  It is ok to
  // stop an already stopped service.
  virtual void Stop() = 0;

  // Returns the maximum amount of time to wait before this call should be
  // called again in milliseconds.
  virtual int Poll() = 0;

  // Process output from child process.
  virtual void ProcessOutput() = 0;

  // Returns if |pid| is a child process of this service.
  virtual bool IsChild(pid_t pid) = 0;

  // Callback when this service has stopped after having started
  // successfully.  |was_error| indicates if an error occurred.
  virtual void OnStopped(bool was_error);

  // Queries if this service is currently running.
  bool is_running() {
    return is_running_;
  }

  // Queries if this service was once running and is now stopped.
  bool was_stopped() {
    return was_stopped_;
  }

  // Set up layering between two service managers |outer| and |inner|.
  // This function may be called multiple times to chain servics together,
  // for instance:
  //   ServiceManager::SetLayerOrder(&turkey, &duck);
  //   ServiceManager::SetLayerOrder(&duck, &chicken);
  static void SetLayerOrder(ServiceManager* outer,
                            ServiceManager* inner) {
    outer->inner_service_ = inner;
    inner->outer_service_ = outer;
  }

  const std::string& service_name() {
    return service_name_;
  }

  // Repeat data from the given |fd| which is assumed to be ready and
  // send it out to syslog, placing |prefix| before each line of
  // output.  Function will block reading fd so it assumes fd is
  // ready.  It will also only read a fixed size per call.  Any
  // partial line read is stored into |partial_line|.  This variable
  // is used on each call to prefix any newly read data.
  static void WriteFdToSyslog(int fd, const std::string& prefix,
                              std::string* partial_line);

 protected:
  friend class IpsecManagerTest;
  friend class L2tpManagerTest;
  friend class ServiceManagerTest;
  FRIEND_TEST(L2tpManagerTest, PollNothingIfRunning);
  FRIEND_TEST(IpsecManagerTest, PollNothingIfRunning);
  FRIEND_TEST(ServiceManagerTest, InitializeDirectories);
  FRIEND_TEST(ServiceManagerTest, OnStoppedFromFailure);
  FRIEND_TEST(ServiceManagerTest, OnStoppedFromSuccess);

  ServiceManager* inner_service() { return inner_service_; }

  ServiceManager* outer_service() { return outer_service_; }

  static const FilePath* temp_path() { return temp_path_; }

 private:
  // Indicates if this service is currently running.
  bool is_running_;

  // Indicates if this service was running and is now stopped.
  bool was_stopped_;

  // Pointer to the next layer or NULL if innermost.
  ServiceManager* inner_service_;

  // Pointer to the outer layer or NULL if outermost.
  ServiceManager* outer_service_;

  // Name of this service.
  std::string service_name_;

  // Path to temporary directory on cryptohome.
  static const FilePath* temp_path_;

  // Path to base directory of temporary directory on cryptohome.
  static const char* temp_base_path_;
};

#endif  // _VPN_MANAGER_SERVICE_MANAGER_H_
