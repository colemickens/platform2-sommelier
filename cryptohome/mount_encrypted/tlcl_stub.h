// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOUNT_ENCRYPTED_TLCL_STUB_H_
#define CRYPTOHOME_MOUNT_ENCRYPTED_TLCL_STUB_H_

#include <stdint.h>

#include <map>
#include <vector>

#include <base/macros.h>

#include <vboot/tlcl.h>

class TlclStub {
 public:
  struct NvramSpaceData {
    uint32_t attributes = 0;
    std::vector<uint8_t> contents;
    bool write_locked = false;
    bool read_locked = false;
  };

  TlclStub();
  ~TlclStub();

  // Get the space data for |index|.
  NvramSpaceData* GetSpace(uint32_t index);

  // Put the TPM into owned state with the specified owner auth secret.
  void SetOwned(const std::vector<uint8_t>& owner_auth);

  // This is used to obtain the current stub instance for servicing Tlcl calls.
  // Do not call directly in tests, but construct your own TlclStub instance
  // which will then be returned by Get().
  static TlclStub* Get();

  // Service functions to handle Tlcl invocations follow.
  uint32_t GetOwnership(uint8_t* owned);

  uint32_t GetRandom(uint8_t* data, uint32_t length, uint32_t* size);

  uint32_t DefineSpace(uint32_t index, uint32_t perm, uint32_t size);
  uint32_t GetPermissions(uint32_t index, uint32_t* permissions);
  uint32_t Write(uint32_t index, const void* data, uint32_t length);
  uint32_t Read(uint32_t index, void* data, uint32_t length);
  uint32_t WriteLock(uint32_t index);
  uint32_t ReadLock(uint32_t index);

 private:
  bool is_owned() const { return !owner_auth_.empty(); }

  template<typename Action>
  uint32_t WithSpace(uint32_t index, Action action);

  std::vector<uint8_t> owner_auth_;
  std::map<uint32_t, NvramSpaceData> nvram_spaces_;

  // The static instance pointer return by Get(). Points at the most recently
  // constructed TlclStub instance.
  static TlclStub* g_instance;

  DISALLOW_COPY_AND_ASSIGN(TlclStub);
};

#endif  // CRYPTOHOME_MOUNT_ENCRYPTED_TLCL_STUB_H_
