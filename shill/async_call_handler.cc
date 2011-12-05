// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/async_call_handler.h"

#include "shill/adaptor_interfaces.h"
#include "shill/error.h"

namespace shill {

AsyncCallHandler::AsyncCallHandler()
    : returner_(NULL) { }

AsyncCallHandler::AsyncCallHandler(ReturnerInterface *returner)
    : returner_(returner) { }

bool AsyncCallHandler::CompleteOperation() {
  DoReturn();
  return true;
}

bool AsyncCallHandler::CompleteOperationWithError(const Error &error) {
  if (returner_) {
    returner_->ReturnError(error);
    returner_ = NULL;
  }
  return true;
}

bool AsyncCallHandler::Complete() {
  return CompleteOperation();
}

bool AsyncCallHandler::Complete(const Error &error) {
  if (error.IsSuccess())
    return CompleteOperation();
  else
    return CompleteOperationWithError(error);
}

void AsyncCallHandler::DoReturn() {
  if (returner_) {
    returner_->Return();
    returner_ = NULL;
  }
}

}  // namespace shill
