// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_NOTIFICATION_NOTIFICATION_PARSER_H_
#define LIBWEAVE_SRC_NOTIFICATION_NOTIFICATION_PARSER_H_

#include <string>

#include <base/values.h>

#include "libweave/src/notification/notification_delegate.h"

namespace buffet {

// Parses the notification JSON object received from GCD server and invokes
// the appropriate method from the |delegate|.
// Returns false if unexpected or malformed notification is received.
bool ParseNotificationJson(const base::DictionaryValue& notification,
                           NotificationDelegate* delegate);

}  // namespace buffet

#endif  // LIBWEAVE_SRC_NOTIFICATION_NOTIFICATION_PARSER_H_
