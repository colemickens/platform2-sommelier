// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_STREAMS_OPENSSL_STREAM_BIO_H_
#define LIBCHROMEOS_CHROMEOS_STREAMS_OPENSSL_STREAM_BIO_H_

#include <chromeos/chromeos_export.h>

// Forward-declare BIO as an alias to OpenSSL's internal bio_st structure.
using BIO = struct bio_st;

namespace chromeos {

class Stream;

// Creates a new BIO that uses the chromeos::Stream as the back-end storage.
// The created BIO does *NOT* own the |stream| and the stream must out-live
// the BIO.
// At the moment, only BIO_read and BIO_write operations are supported as well
// as BIO_flush. More functionality could be added to this when/if needed.
// The returned BIO performs *NON-BLOCKING* IO on the underlying stream.
CHROMEOS_EXPORT BIO* BIO_new_stream(chromeos::Stream* stream);

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_STREAMS_OPENSSL_STREAM_BIO_H_
