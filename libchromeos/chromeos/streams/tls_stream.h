// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_STREAMS_TLS_STREAM_H_
#define LIBCHROMEOS_CHROMEOS_STREAMS_TLS_STREAM_H_

#include <memory>

#include <base/macros.h>
#include <chromeos/chromeos_export.h>
#include <chromeos/errors/error.h>
#include <chromeos/streams/stream.h>

// Forward-declarations of some of OpenSSL types.
using X509 = struct x509_st;
using EVP_PKEY = struct evp_pkey_st;

namespace chromeos {

class CHROMEOS_EXPORT TlsStream : public Stream {
 public:
  ~TlsStream() override;

  // Perform a TLS handshake and establish secure connection over |socket|.
  // Calls |callback| when successful and passes the instance of TlsStream
  // as an argument. In case of an error, |error_callback| is called.
  // It uses the specified |certificate| and |private_key| in TLS negotiations
  // and data encryption.
  static void Connect(
      StreamPtr socket,
      X509* certificate,
      EVP_PKEY* private_key,
      const base::Callback<void(StreamPtr)>& success_callback,
      const Stream::ErrorCallback& error_callback);

  // Overrides from Stream:
  bool IsOpen() const override;
  bool CanRead() const override { return true; }
  bool CanWrite() const override { return true; }
  bool CanSeek() const override  { return false; }
  bool CanGetSize() const override { return false; }
  uint64_t GetSize() const override { return 0; }
  bool SetSizeBlocking(uint64_t size, ErrorPtr* error) override;
  uint64_t GetRemainingSize() const override { return 0; }
  uint64_t GetPosition() const override { return 0; }
  bool Seek(int64_t offset,
            Whence whence,
            uint64_t* new_position,
            ErrorPtr* error) override;
  bool ReadNonBlocking(void* buffer,
                       size_t size_to_read,
                       size_t* size_read,
                       bool* end_of_stream,
                       ErrorPtr* error) override;
  bool WriteNonBlocking(const void* buffer,
                        size_t size_to_write,
                        size_t* size_written,
                        ErrorPtr* error) override;
  bool FlushBlocking(ErrorPtr* error) override;
  bool CloseBlocking(ErrorPtr* error) override;
  bool WaitForData(AccessMode mode,
                   const base::Callback<void(AccessMode)>& callback,
                   ErrorPtr* error) override;
  bool WaitForDataBlocking(AccessMode in_mode,
                           AccessMode* out_mode,
                           ErrorPtr* error) override;

 private:
  class TlsStreamImpl;

  // Private constructor called from TlsStream::Connect() factory method.
  explicit TlsStream(std::unique_ptr<TlsStreamImpl> impl);

  std::unique_ptr<TlsStreamImpl> impl_;
  DISALLOW_COPY_AND_ASSIGN(TlsStream);
};

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_STREAMS_TLS_STREAM_H_
