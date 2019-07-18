// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_EAP_CREDENTIALS_H_
#define SHILL_MOCK_EAP_CREDENTIALS_H_

#include <string>

#include "shill/eap_credentials.h"

#include <gmock/gmock.h>

namespace shill {

class MockEapCredentials : public EapCredentials {
 public:
  MockEapCredentials();
  ~MockEapCredentials() override;

  MOCK_CONST_METHOD0(IsConnectable, bool());
  MOCK_CONST_METHOD0(IsConnectableUsingPassphrase, bool());
  MOCK_METHOD2(Load, void(StoreInterface* store, const std::string& id));
  MOCK_CONST_METHOD2(OutputConnectionMetrics,
                     void(Metrics* metrics, Technology technology));
  MOCK_CONST_METHOD2(PopulateSupplicantProperties,
                     void(CertificateFile* certificate_file,
                          KeyValueStore* params));
  MOCK_CONST_METHOD3(Save, void(
      StoreInterface* store, const std::string& id, bool save_credentials));
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD2(SetKeyManagement, bool(const std::string& key_management,
                                      Error* error));
  MOCK_CONST_METHOD0(identity, const std::string&());
  MOCK_CONST_METHOD0(key_management, const std::string&());
  MOCK_METHOD1(set_password, void(const std::string& password));
  MOCK_CONST_METHOD0(pin, const std::string&());

 private:
  std::string kDefaultKeyManagement;

  DISALLOW_COPY_AND_ASSIGN(MockEapCredentials);
};

}  // namespace shill

#endif  // SHILL_MOCK_EAP_CREDENTIALS_H_
