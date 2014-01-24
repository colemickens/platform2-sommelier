// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_KEY_GENERATOR_H_
#define LOGIN_MANAGER_KEY_GENERATOR_H_

#include <glib.h>

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "login_manager/generator_job.h"

namespace login_manager {

class ProcessManagerServiceInterface;
class SystemUtils;

class KeyGenerator {
 public:
  KeyGenerator(SystemUtils* utils, ProcessManagerServiceInterface* manager);
  virtual ~KeyGenerator();

  // Start the generation of a new Owner keypair for |username| as |uid|.
  // Upon success, hands off ownership of the key generation job to |manager_|
  // and returns true.
  // The username of the key owner and temporary storage location of the
  // generated public key are stored internally until Reset() is called.
  virtual bool Start(const std::string& username, uid_t uid);

  void InjectJobFactory(scoped_ptr<GeneratorJobFactoryInterface> factory);

  void HandleExit(bool success);

 private:
  FRIEND_TEST(KeyGeneratorTest, KeygenEndToEndTest);
  static const char kTemporaryKeyFilename[];

  // Clear per-generation state.
  void Reset();

  SystemUtils *utils_;
  ProcessManagerServiceInterface* manager_;

  scoped_ptr<GeneratorJobFactoryInterface> factory_;
  scoped_ptr<GeneratorJobInterface> keygen_job_;
  bool generating_;
  std::string key_owner_username_;
  std::string temporary_key_filename_;
  DISALLOW_COPY_AND_ASSIGN(KeyGenerator);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_KEY_GENERATOR_H_
