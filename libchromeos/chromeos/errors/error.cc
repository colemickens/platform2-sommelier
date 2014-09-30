// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/errors/error.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

using chromeos::Error;
using chromeos::ErrorPtr;

ErrorPtr Error::Create(const std::string& domain,
                       const std::string& code,
                       const std::string& message) {
  return Create(domain, code, message, ErrorPtr());
}

ErrorPtr Error::Create(const std::string& domain,
                       const std::string& code,
                       const std::string& message,
                       ErrorPtr inner_error) {
  LOG(ERROR) << "Error::Create: Domain=" << domain
             << ", Code=" << code << ", Message=" << message;
  return ErrorPtr(new Error(domain, code, message, std::move(inner_error)));
}

void Error::AddTo(ErrorPtr* error, const std::string& domain,
                  const std::string& code, const std::string& message) {
  if (error) {
    *error = Create(domain, code, message, std::move(*error));
  } else {
    // Create already logs the error, but if |error| is nullptr,
    // we still want to log the error...
    LOG(ERROR) << "Error::Create: Domain=" << domain
               << ", Code=" << code << ", Message=" << message;
  }
}

void Error::AddToPrintf(ErrorPtr* error, const std::string& domain,
                        const std::string& code, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string message = base::StringPrintV(format, ap);
  va_end(ap);
  AddTo(error, domain, code, message);
}

bool Error::HasDomain(const std::string& domain) const {
  return FindErrorOfDomain(this, domain) != nullptr;
}

bool Error::HasError(const std::string& domain, const std::string& code) const {
  return FindError(this, domain, code) != nullptr;
}

const Error* Error::GetFirstError() const {
  const Error* err = this;
  while (err->GetInnerError())
    err = err->GetInnerError();
  return err;
}

Error::Error(const std::string& domain, const std::string& code,
             const std::string& message, ErrorPtr inner_error) :
    domain_(domain), code_(code), message_(message),
    inner_error_(std::move(inner_error)) {
}

const Error* Error::FindErrorOfDomain(const Error* error_chain_start,
                                      const std::string& domain) {
  while (error_chain_start) {
    if (error_chain_start->GetDomain() == domain)
      break;
    error_chain_start = error_chain_start->GetInnerError();
  }
  return error_chain_start;
}

const Error* Error::FindError(const Error* error_chain_start,
                              const std::string& domain,
                              const std::string& code) {
  while (error_chain_start) {
    if (error_chain_start->GetDomain() == domain &&
        error_chain_start->GetCode() == code)
      break;
    error_chain_start = error_chain_start->GetInnerError();
  }
  return error_chain_start;
}
