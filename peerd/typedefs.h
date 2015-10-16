// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_TYPEDEFS_H_
#define PEERD_TYPEDEFS_H_

#include <brillo/dbus/async_event_sequencer.h>

namespace peerd {

using CompletionAction =
    brillo::dbus_utils::AsyncEventSequencer::CompletionAction;

extern const char kPeerdErrorDomain[];

}  // namespace peerd

#endif  // PEERD_TYPEDEFS_H_
