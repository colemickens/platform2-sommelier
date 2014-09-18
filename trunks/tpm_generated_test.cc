// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: These tests are not generated. They test generated code.

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <gtest/gtest.h>

#include "trunks/mock_authorization_delegate.h"
#include "trunks/mock_command_transceiver.h"
#include "trunks/tpm_generated.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::WithArg;

namespace trunks {

// This test is designed to get good coverage of the different types of code
// generated for serializing and parsing structures / unions / typedefs.
TEST(GeneratorTest, SerializeParseStruct) {
  TPM2B_CREATION_DATA data;
  memset(&data, 0, sizeof(TPM2B_CREATION_DATA));
  data.creation_data.pcr_select.count = 1;
  data.creation_data.pcr_select.pcr_selections[0].hash = TPM_ALG_SHA256;
  data.creation_data.pcr_select.pcr_selections[0].sizeof_select = 1;
  data.creation_data.pcr_select.pcr_selections[0].pcr_select[0] = 0;
  data.creation_data.pcr_digest.size = 2;
  data.creation_data.locality = 0;
  data.creation_data.parent_name_alg = TPM_ALG_SHA256;
  data.creation_data.parent_name.size = 3;
  data.creation_data.parent_qualified_name.size = 4;
  data.creation_data.outside_info.size = 5;
  std::string buffer;
  TPM_RC rc = Serialize_TPM2B_CREATION_DATA(data, &buffer);
  ASSERT_EQ(TPM_RC_SUCCESS, rc);
  EXPECT_EQ(35, buffer.size());
  TPM2B_CREATION_DATA data2;
  memset(&data2, 0, sizeof(TPM2B_CREATION_DATA));
  std::string buffer_before = buffer;
  std::string buffer_parsed;
  rc = Parse_TPM2B_CREATION_DATA(&buffer, &data2, &buffer_parsed);
  ASSERT_EQ(TPM_RC_SUCCESS, rc);
  EXPECT_EQ(0, buffer.size());
  EXPECT_EQ(buffer_before, buffer_parsed);
  EXPECT_EQ(0, memcmp(&data, &data2, sizeof(TPM2B_CREATION_DATA)));
}

TEST(GeneratorTest, SerializeBufferOverflow) {
  TPM2B_MAX_BUFFER value;
  value.size = arraysize(value.buffer) + 1;
  std::string tmp;
  EXPECT_EQ(TPM_RC_INSUFFICIENT, Serialize_TPM2B_MAX_BUFFER(value, &tmp));
}

TEST(GeneratorTest, ParseBufferOverflow) {
  TPM2B_MAX_BUFFER tmp;
  // Case 1: Sufficient source but overflow the destination.
  std::string malformed1 = "\x10\x00";
  malformed1 += std::string(0x1000, 'A');
  ASSERT_GT(0x1000, sizeof(tmp.buffer));
  EXPECT_EQ(TPM_RC_INSUFFICIENT,
            Parse_TPM2B_MAX_BUFFER(&malformed1, &tmp, NULL));
  // Case 2: Sufficient destination but overflow the source.
  std::string malformed2 = "\x00\x01";
  EXPECT_EQ(TPM_RC_INSUFFICIENT,
            Parse_TPM2B_MAX_BUFFER(&malformed2, &tmp, NULL));
}

// A fixture for asynchronous command flow tests.
class CommandFlowTest : public testing::Test {
 public:
  CommandFlowTest() : response_code_(TPM_RC_SUCCESS) {}
  virtual ~CommandFlowTest() {}

  void StartupCallback(TPM_RC response_code) {
    response_code_ = response_code;
  }

  void CertifyCallback(TPM_RC response_code,
                       TPM2B_ATTEST certify_info,
                       TPMT_SIGNATURE signature) {
    response_code_ = response_code;
    signed_data_ = StringFrom_TPM2B_ATTEST(certify_info);
    signature_ = StringFrom_TPM2B_PUBLIC_KEY_RSA(
        signature.signature.rsassa.sig);
  }

 protected:
  void Run() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  TPM_RC response_code_;
  std::string signature_;
  std::string signed_data_;
};

// A functor for posting command responses. This is different than invoking the
// callback directly (e.g. via InvokeArgument) in that the original call will
// return before the response callback is invoked. This more closely matches how
// this code is expected to work when integrated.
class PostResponse {
 public:
  explicit PostResponse(const std::string& response) : response_(response) {}
  void operator() (const base::Callback<void(const std::string&)>& callback) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, response_));
  }

 private:
  std::string response_;
};

// A functor to handle fake encryption / decryption of parameters.
class Encryptor {
 public:
  Encryptor(const std::string& expected_input, const std::string& output)
      : expected_input_(expected_input),
        output_(output) {}
  bool operator() (std::string* value) {
    EXPECT_EQ(expected_input_, *value);
    value->assign(output_);
    return true;
  }

 private:
  std::string expected_input_;
  std::string output_;
};

TEST_F(CommandFlowTest, SimpleCommandFlow) {
  // A hand-rolled TPM2_Startup command.
  std::string expected_command("\x80\x01"          // tag=TPM_ST_NO_SESSIONS
                               "\x00\x00\x00\x0C"  // size=12
                               "\x00\x00\x01\x44"  // code=TPM_CC_Startup
                               "\x00\x00",         // param=TPM_SU_CLEAR
                               12);
  std::string command_response("\x80\x01"           // tag=TPM_ST_NO_SESSIONS
                               "\x00\x00\x00\x0A"   // size=10
                               "\x00\x00\x00\x00",  // code=TPM_RC_SUCCESS
                               10);
  StrictMock<MockCommandTransceiver> transceiver;
  EXPECT_CALL(transceiver, SendCommand(expected_command, _))
    .WillOnce(WithArg<1>(Invoke(PostResponse(command_response))));
  StrictMock<MockAuthorizationDelegate> authorization;
  EXPECT_CALL(authorization, GetCommandAuthorization(_, _))
    .WillOnce(Return(true));
  Tpm tpm(&transceiver);
  response_code_ = TPM_RC_FAILURE;
  tpm.Startup(TPM_SU_CLEAR,
              &authorization,
              base::Bind(&CommandFlowTest::StartupCallback,
                         base::Unretained(this)));
  Run();
  EXPECT_EQ(TPM_RC_SUCCESS, response_code_);
}

TEST_F(CommandFlowTest, SimpleCommandFlowWithError) {
  // A hand-rolled TPM2_Startup command.
  std::string expected_command("\x80\x01"          // tag=TPM_ST_NO_SESSIONS
                               "\x00\x00\x00\x0C"  // size=12
                               "\x00\x00\x01\x44"  // code=TPM_CC_Startup
                               "\x00\x00",         // param=TPM_SU_CLEAR
                               12);
  std::string command_response("\x80\x01"           // tag=TPM_ST_NO_SESSIONS
                               "\x00\x00\x00\x0A"   // size=10
                               "\x00\x00\x01\x01",  // code=TPM_RC_FAILURE
                               10);
  StrictMock<MockCommandTransceiver> transceiver;
  EXPECT_CALL(transceiver, SendCommand(expected_command, _))
    .WillOnce(WithArg<1>(Invoke(PostResponse(command_response))));
  StrictMock<MockAuthorizationDelegate> authorization;
  EXPECT_CALL(authorization, GetCommandAuthorization(_, _))
    .WillOnce(Return(true));
  Tpm tpm(&transceiver);
  tpm.Startup(TPM_SU_CLEAR,
              &authorization,
              base::Bind(&CommandFlowTest::StartupCallback,
                         base::Unretained(this)));
  Run();
  EXPECT_EQ(TPM_RC_FAILURE, response_code_);
}

// This test is designed to get good coverage of the different types of code
// generated for command / response processing. It covers:
// - input handles
// - authorization
// - multiple input and output parameters
// - parameter encryption and decryption
TEST_F(CommandFlowTest, FullCommandFlow) {
  // A hand-rolled TPM2_Certify command.
  std::string auth_in(10, 'A');
  std::string auth_out(20, 'B');
  std::string user_data("\x00\x0C" "ct_user_data", 14);
  std::string scheme("\x00\x10", 2);  // scheme=TPM_ALG_NULL
  std::string signed_data("\x00\x0E" "ct_signed_data", 16);
  std::string signature("\x00\x14"    // sig_scheme=RSASSA
                        "\x00\x0B"    // hash_scheme=SHA256
                        "\x00\x09"    // signature size
                        "signature",  // signature bytes
                        15);
  std::string expected_command("\x80\x02"           // tag=TPM_ST_SESSIONS
                               "\x00\x00\x00\x30"   // size=48
                               "\x00\x00\x01\x48"   // code=TPM_CC_Certify
                               "\x11\x22\x33\x44"   // @objectHandle
                               "\x55\x66\x77\x88"   // @signHandle
                               "\x00\x00\x00\x0A",  // auth_size=10
                               22);
  expected_command += auth_in + user_data + scheme;
  std::string command_response("\x80\x02"           // tag=TPM_ST_SESSIONS
                               "\x00\x00\x00\x41"   // size=65
                               "\x00\x00\x00\x00"   // code=TPM_RC_SUCCESS
                               "\x00\x00\x00\x1F",  // param_size=31
                               14);
  command_response += signed_data + signature + auth_out;

  StrictMock<MockCommandTransceiver> transceiver;
  EXPECT_CALL(transceiver, SendCommand(expected_command, _))
    .WillOnce(WithArg<1>(Invoke(PostResponse(command_response))));
  StrictMock<MockAuthorizationDelegate> authorization;
  EXPECT_CALL(authorization, GetCommandAuthorization(_, _))
    .WillOnce(DoAll(SetArgPointee<1>(auth_in), Return(true)));
  EXPECT_CALL(authorization, CheckResponseAuthorization(_, auth_out))
    .WillOnce(Return(true));
  EXPECT_CALL(authorization, EncryptCommandParameter(_))
    .WillOnce(Invoke(Encryptor("pt_user_data", "ct_user_data")));
  EXPECT_CALL(authorization, DecryptResponseParameter(_))
    .WillOnce(Invoke(Encryptor("ct_signed_data", "pt_signed_data")));

  TPMT_SIG_SCHEME null_scheme;
  null_scheme.scheme = TPM_ALG_NULL;
  null_scheme.details.rsassa.hash_alg = TPM_ALG_SHA256;
  Tpm tpm(&transceiver);
  tpm.Certify(0x11223344u, "object_handle",
              0x55667788u, "sign_handle",
              Make_TPM2B_DATA("pt_user_data"),
              null_scheme,
              &authorization,
              base::Bind(&CommandFlowTest::CertifyCallback,
                         base::Unretained(this)));
  Run();
  ASSERT_EQ(TPM_RC_SUCCESS, response_code_);
  EXPECT_EQ("pt_signed_data", signed_data_);
  EXPECT_EQ("signature", signature_);
}

}  // namespace trunks
