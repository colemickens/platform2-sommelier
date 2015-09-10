// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_SOCKET_STREAM_H_
#define BUFFET_SOCKET_STREAM_H_

#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <chromeos/streams/stream.h>
#include <weave/stream.h>

namespace buffet {

class SocketStream : public weave::Stream {
 public:
  explicit SocketStream(chromeos::StreamPtr ptr) : ptr_{std::move(ptr)} {}

  ~SocketStream() override = default;

  bool ReadAsync(
      void* buffer,
      size_t size_to_read,
      const base::Callback<void(size_t)>& success_callback,
      const base::Callback<void(const weave::Error*)>& error_callback,
      weave::ErrorPtr* error) override;

  bool WriteAllAsync(
      const void* buffer,
      size_t size_to_write,
      const base::Closure& success_callback,
      const base::Callback<void(const weave::Error*)>& error_callback,
      weave::ErrorPtr* error) override;

  void CancelPendingAsyncOperations() override;

  static std::unique_ptr<weave::Stream> ConnectBlocking(const std::string& host,
                                                        uint16_t port);

  static void TlsConnect(
      std::unique_ptr<weave::Stream> socket,
      const std::string& host,
      const base::Callback<void(std::unique_ptr<weave::Stream>)>&
          success_callback,
      const base::Callback<void(const weave::Error*)>& error_callback);

 private:
  chromeos::StreamPtr ptr_;
  DISALLOW_COPY_AND_ASSIGN(SocketStream);
};

}  // namespace buffet

#endif  // BUFFET_SOCKET_STREAM_H_
