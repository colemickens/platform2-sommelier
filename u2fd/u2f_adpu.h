// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_U2F_ADPU_H_
#define U2FD_U2F_ADPU_H_

#include <stdint.h>
#include <string>
#include <vector>

#include <base/optional.h>

#include "u2fd/util.h"

// Classes for dealing with command and response ADPUs, as described in the "U2F
// Raw Message Formats" specification.

namespace u2f {

// INS codes used in U2F Command ADPUs.
enum class U2fIns : uint8_t {
  kU2fRegister = 1,       // U2F_REGISTER
  kU2fAuthenticate = 2,   // U2F_AUTHENTICATE
  kU2fVersion = 3,        // U2F_VERSION
  kU2fAttestCert = 0xbe,  // Vendor command to retrieve G2F certificate.
  kInsInvalid = 0xff,
};

// Represents a command ADPU.
class U2fCommandAdpu {
 public:
  // Fixed-size header of a command ADPU.
  struct Header {
    U2fIns ins_;
    uint8_t p1_;
    uint8_t p2_;
  };

  // Attempts to parse the specified string as an ADPU, and returns a valid
  // U2fCommandAdpu if successful, or base::nullopt otherwise.
  static base::Optional<U2fCommandAdpu> ParseFromString(
      const std::string& adpu_raw);

  // Creates an 'empty' ADPU for the command with the specified INS command
  // code.
  static U2fCommandAdpu CreateForU2fIns(U2fIns ins);

  // Returns the INS command code for this ADPU.
  U2fIns Ins() const { return header_.ins_; }
  // Returns the P1 parameter for this ADPU.
  uint8_t P1() const { return header_.p1_; }
  // Returns the request body for this ADPU.
  const std::string& Body() const { return data_; }
  // Returns the max response length for this ADPU.
  uint32_t MaxResponseLength() const { return max_response_length_; }
  // Serializes this ADPU to a string.
  std::string ToString();

 protected:
  Header header_;
  std::string data_;
  uint32_t max_response_length_;

 private:
  // Private constructor, use factory methods above.
  U2fCommandAdpu() = default;
  // Internal parser class called by ParseFromString().
  class Parser;
  friend class Parser;
};

// Represents an ADPU for a U2F_REGISTER request.
class U2fRegisterRequestAdpu {
 public:
  // Attempt to parse the body of the specified ADPU as a U2F_REGISTER request.
  // Returns a valid U2fRegisterRequestAdpu if successful, or base::nullopt
  // otherwise.
  static base::Optional<U2fRegisterRequestAdpu> FromCommandAdpu(
      const U2fCommandAdpu& adpu);

  // Whether the request response should use the G2F attestation certificate (if
  // available).
  bool UseG2fAttestation() const { return g2f_attestation_; }

  // Accessors for the request fields.
  const std::vector<uint8_t>& GetAppId() const { return app_id_; }
  const std::vector<uint8_t>& GetChallenge() const { return challenge_; }

 private:
  bool g2f_attestation_;
  std::vector<uint8_t> app_id_;
  std::vector<uint8_t> challenge_;
};

class U2fAuthenticateRequestAdpu {
 public:
  // Attempt to parse the body of the specified ADPU as a U2F_AUTHENTICATE
  // request. Returns a valid U2fRegisterRequestAdpu if successful, or
  // base::nullopt otherwise.
  static base::Optional<U2fAuthenticateRequestAdpu> FromCommandAdpu(
      const U2fCommandAdpu& adpu);

  // Returns true if the ADPU is for a U2F_AUTHENTICATE check-only
  // request. Check-only requests should verify whether the specified key handle
  // is owned by this U2F device, but not perform any authentication.
  bool IsAuthenticateCheckOnly() { return auth_check_only_; }

  // Accessors for the request fields.
  const std::vector<uint8_t>& GetAppId() const { return app_id_; }
  const std::vector<uint8_t>& GetChallenge() const { return challenge_; }
  const std::vector<uint8_t>& GetKeyHandle() const { return key_handle_; }

 private:
  bool auth_check_only_;
  std::vector<uint8_t> app_id_;
  std::vector<uint8_t> challenge_;
  std::vector<uint8_t> key_handle_;
};

// Represents a response ADPU. Provides methods for building and serializing a
// response.
class U2fResponseAdpu {
 public:
  // Constructs an empty response.
  U2fResponseAdpu() = default;

  // Serialize the response to the specified string.
  bool ToString(std::string* out);

  // Methods to append data to the response.
  void AppendByte(uint8_t byte) { data_.push_back(byte); }
  void AppendBytes(const std::vector<uint8_t>& bytes) {
    util::AppendToVector(bytes, &data_);
  }
  void AppendString(const std::string& string) {
    util::AppendToVector(string, &data_);
  }
  template <typename T>
  void AppendObject(const T& obj) {
    util::AppendToVector(obj, &data_);
  }

  // Sets the return status for the response.
  void SetStatus(uint16_t sw) {
    sw1_ = sw >> 8;
    sw2_ = static_cast<uint8_t>(sw);
  }

 private:
  std::vector<uint8_t> data_;
  uint8_t sw1_;
  uint8_t sw2_;
};

}  // namespace u2f

#endif  // U2FD_U2F_ADPU_H_
