// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_KEY_CHALLENGE_SERVICE_H_
#define CRYPTOHOME_MOCK_KEY_CHALLENGE_SERVICE_H_

#include <string>

#include <base/callback.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

#include "cryptohome/key_challenge_service.h"

#include "key.pb.h"  // NOLINT(build/include)
#include "rpc.pb.h"  // NOLINT(build/include)

namespace cryptohome {

class MockKeyChallengeService : public KeyChallengeService {
 public:
  MockKeyChallengeService();
  ~MockKeyChallengeService() override;

  MOCK_METHOD3(ChallengeKey,
               void(const AccountIdentifier&,
                    const KeyChallengeRequest&,
                    const ResponseCallback&));
};

// Helper class for simplifying the use of MockKeyChallengeService.
//
// It encapsulates setting up a mock expectation and execution of the callback
// with which the mocked method was called. Intended usage: first call
// ExpectSignatureChallenge(), and then, after the mocked method gets executed,
// call one of the Simulate*() methods.
class KeyChallengeServiceMockController final {
 public:
  explicit KeyChallengeServiceMockController(
      MockKeyChallengeService* mock_service);
  ~KeyChallengeServiceMockController();

  // Sets up a mock expectation on ChallengeKey(). This mock expectation doesn't
  // run the passed ResponseCallback, but remembers it in
  // |intercepted_response_callback_|, allowing the later call of a Simulate*()
  // method.
  void ExpectSignatureChallenge(
      const std::string& expected_username,
      const brillo::Blob& expected_public_key_spki_der,
      const brillo::Blob& expected_challenge_value,
      ChallengeSignatureAlgorithm expected_signature_algorithm);

  // Whether the mocked ChallengeKey() has been called.
  //
  // It's allowed to call the Simulate*() methods only after this returns true.
  bool is_challenge_requested() const {
    return !intercepted_response_callback_.is_null();
  }

  // Simulates a successful response for the given challenge request.
  void SimulateSignatureChallengeResponse(const brillo::Blob& signature_value);
  // Simulates a failed response for the given challenge request.
  void SimulateFailureResponse();

 private:
  // Not owned.
  MockKeyChallengeService* const mock_service_;
  // The response callback that was passed to the mocked ChallengeKey() method.
  KeyChallengeService::ResponseCallback intercepted_response_callback_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_KEY_CHALLENGE_SERVICE_H_
