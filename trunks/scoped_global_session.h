//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef TRUNKS_SCOPED_GLOBAL_SESSION_H_
#define TRUNKS_SCOPED_GLOBAL_SESSION_H_

#include <memory>
#include <utility>

#include <base/logging.h>
#include <base/macros.h>

#include "trunks/error_codes.h"
#include "trunks/hmac_session.h"
#include "trunks/trunks_factory.h"

namespace trunks {

// TODO(http://crbug.com/473843): restore using one global session without
// restarting, when session handles virtualization is supported by trunks.
#define TRUNKS_USE_PER_OP_SESSIONS

// Helper class for handling global HMAC sessions. Until resource manager
// supports handles virtualization, global sessions should not be used:
// a session handle may be flushed after a system is suspended.
// To support cases when daemons create a global session as
// std::unique_ptr<HmacSession> during initialization and then reuse it over
// the lifetime of the daemon, each operation that calls such |global_session_|
// should before use define a scoped hmac session variable:
// ScopedGlobalHmacSession(<factory-ptr>, <enable-encryption>, &global_session_)
#ifdef TRUNKS_USE_PER_OP_SESSIONS
class ScopedGlobalHmacSession {
 public:
  ScopedGlobalHmacSession(const TrunksFactory* factory,
      bool enable_encryption,
      std::unique_ptr<HmacSession>* session)
      : target_session_(session) {
    DCHECK(target_session_);
    VLOG_IF(1, *target_session_) << "Concurrent sessions?";
    std::unique_ptr<HmacSession> new_session = factory->GetHmacSession();
    TPM_RC result = new_session->StartUnboundSession(enable_encryption);
    if (result != TPM_RC_SUCCESS) {
      LOG(ERROR) << "Error starting an authorization session: "
                 << GetErrorString(result);
      *target_session_ = nullptr;
    } else {
      *target_session_ = std::move(new_session);
    }
  }

  ~ScopedGlobalHmacSession() {
    *target_session_ = nullptr;
  }

 private:
  std::unique_ptr<HmacSession> *target_session_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGlobalHmacSession);
};
#else  // TRUNKS_USE_PER_OP_SESSIONS
class ScopedGlobalHmacSession {
 public:
  ScopedGlobalHmacSession(const TrunksFactory* /* factory */,
      bool /* enable_encryption */,
      std::unique_ptr<HmacSession>* /* session */) {}
 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedGlobalHmacSession);
};
#endif  // TRUNKS_USE_PER_OP_SESSIONS

}  // namespace trunks

#endif  // TRUNKS_SCOPED_GLOBAL_SESSION_H_
