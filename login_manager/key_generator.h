// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_KEY_GENERATOR_H_
#define LOGIN_MANAGER_KEY_GENERATOR_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace login_manager {

class ChildJobInterface;
class MockChildJob;
class SessionManagerService;

class KeyGenerator {
 public:
  static const char kTemporaryKeyFilename[];

  KeyGenerator();
  virtual ~KeyGenerator();

  // Use |key| to start the generation of a new Owner keypair as user |uid|.
  // Upon success, hands off ownership of the key generation job to |manager|
  // and returns true.
  virtual bool Start(uid_t uid, SessionManagerService* manager);

  void InjectMockKeygenJob(MockChildJob* keygen);  // Takes ownership.
 private:
  // Forks a process for |job| and returns the PID.
  virtual int RunJob(ChildJobInterface* job);

  static const char kKeygenExecutable[];

  scoped_ptr<ChildJobInterface> keygen_job_;
  DISALLOW_COPY_AND_ASSIGN(KeyGenerator);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_KEY_GENERATOR_H_
