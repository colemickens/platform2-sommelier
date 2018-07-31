// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_ESIM_H_
#define HERMES_ESIM_H_

#include <cstdint>
#include <vector>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>

namespace hermes {

// EsimError will be the generic abstraction from specific protocol errors to an
// error that the interface can understand. This should contain a set of errors
// that are recoverable, meaning if they occur, the process can still complete
// successfully. This should also include errors that are fatal, and should be
// reported back to the user.
enum class EsimError {
  kEsimSuccess,       // success condition
  kEsimError,         // fatal
  kEsimNotConnected,  // non-fatal
};

// Provides an interface through which the LPD can communicate with the eSIM
// chip. This is responsible for opening, maintaining, and closing the logical
// channel that will be opened to the chip.
class Esim {
 public:
  using ErrorCallback = base::Callback<void(EsimError error_data)>;
  using DataCallback =
      base::Callback<void(const std::vector<uint8_t>& esim_data)>;

  virtual ~Esim() = default;

  virtual void Initialize(const base::Closure& success_callback,
                          const ErrorCallback& error_callback) = 0;

  // Makes eSIM API call to request the eSIM to open a logical channel to
  // communicate through. This will be the method through which all two way
  // communication will be dealt with the hardware. Calls |data_callback|
  // on successful open channel, or |error_callback| on error.
  //
  // Parameters
  //  data_callback   - function to call on successful channel opening
  //  error_callback  - function to handle error returned from eSIM
  virtual void OpenLogicalChannel(const DataCallback& data_callback,
                                  const ErrorCallback& error_callback) = 0;

  // Makes eSIM API call to request the eSIM to return either the info1 or
  // the info2 block of data to send to the SM-DP+ server to begin
  // Authentication. Calls |callback| with the newly returned data, or
  // |error_callback| on error.
  //
  // Parameters
  //  which          - specify either the info1 or info2 data
  //  callback       - function to call on successful return of specified eSIM
  //                   info data
  //  error_callback - function to handle error returned from eSIM
  virtual void GetInfo(int which,
                       const DataCallback& data_callback,
                       const ErrorCallback& error_callback) = 0;
  // Makes eSIM API call to request the eSIM to return the eSIM Challenge,
  // which is the second parameter needed to begin Authentication with the
  // SM-DP+ server. Calls |callback| on returned challenge, or |error_callback|
  // on error.
  //
  // Parameters
  //  callback       - function to call on successful return of eSIM challenge
  //                   data blob
  //  error_callback - function to handle error returned from eSIM
  virtual void GetChallenge(const DataCallback& callback,
                            const ErrorCallback& error_callback) = 0;

  // Makes eSIM API call to request eSIM to authenticate server's signature,
  // which is supplied in server_signature. On success, call |callback| with
  // the eSIM's response. If the authentication fails, call |error_callback|.
  //
  // Parameters
  //  server_signed1            - data that has been signed with
  //                              |server_signature| server_signature SM-DP+
  //                              encryption signature
  //  euicc_ci_pk_id_to_be_used - list of public keys for the eSIM to choose
  //  server_certificate        - SM-DP+ certificate
  //  data_callback             - function to call with the data returned from
  //                              the eSIM on successful authentication of
  //                              |server_data|
  //  error_callback            - function to call if |server_data| is
  //                              determined to be invalid by eSIM chip
  virtual void AuthenticateServer(
      const std::vector<uint8_t>& server_signed1,
      const std::vector<uint8_t>& server_signature1,
      const std::vector<uint8_t>& euicc_ci_pk_id_to_be_used,
      const std::vector<uint8_t>& server_certificate,
      const DataCallback& data_callback,
      const ErrorCallback& error_callback) = 0;
};

}  // namespace hermes

#endif  // HERMES_ESIM_H_
