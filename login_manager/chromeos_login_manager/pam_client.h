// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PAM_CLIENT_H_
#define PAM_CLIENT_H_

#include <security/pam_appl.h>
#include <string>
#include "base/basictypes.h"

namespace chromeos {
// PamClient is a simple client interface to a pam library.  It starts its
// conversation in its constructor and ends it in its destructor.  The
// data passed into the constructor is used during authenticate and can
// be changed outside
class PamClient {
 public:
  // Struct representing the username / password.  This is assumed to
  // be passed to the callback function
  struct UserCredentials {
    std::string username;
    std::string password;
  };

  // Constants used in setting pam environment.  See Init().
  static const char kDisplayName[];
  static const char kLocalUser[];
  static const char kLocalHost[];

  PamClient();

  // Calls pam_start and initializes the pam environment
  virtual void Init(const std::string& service_name);

  // Calls pam_end and frees memory
  virtual ~PamClient();

  // Starts the authentication loop.  This initiates the call to the
  // pam library and back to the conversation callback before returning
  // Returns true on success, false on error
  virtual bool Authenticate(const std::string& username,
                            const std::string& password);

  // Sets the credentials obtained from authenticate and starts a session
  // with the pam library
  virtual bool StartSession();

  // Unsets the credentials and closes started sessio
  virtual bool CloseSession();

  // Returns the last value from the last pam call.  Useful to look
  // at if other calls return false
  virtual int GetLastPamResult() {  return last_pam_result_;  }

  // Static callback for pam conversation.
  static int PamConversationCallback(int num_msg,
                                     const struct pam_message** msg,
                                     struct pam_response** resp,
                                     void* credentials);

 private:
  // Initialized with the constructor and destroyed with the destructor
  pam_handle_t* pam_handle_;

  struct pam_conv pam_conversation_callback_;
  struct UserCredentials user_credentials_;
  int last_pam_result_;

  DISALLOW_COPY_AND_ASSIGN(PamClient);
};

class MockPamClient : public PamClient {
 public:
  MockPamClient() {}
  virtual ~MockPamClient() {}
  virtual void Init(const std::string& service_name) {}
  virtual bool Authenticate(const std::string& username,
			    const std::string& password) {
    return true;
  }
  virtual bool StartSession() { return true; }
  virtual bool CloseSession() { return true; }
  virtual int GetLastPamResult() {  return 0;  }
};

}  // namespace chromeos

#endif /* PAM_CLIENT_H_ */
