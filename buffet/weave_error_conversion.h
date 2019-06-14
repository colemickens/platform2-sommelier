// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_WEAVE_ERROR_CONVERSION_H_
#define BUFFET_WEAVE_ERROR_CONVERSION_H_

#include <memory>
#include <string>

#include <base/location.h>
#include <brillo/errors/error.h>
#include <weave/error.h>

namespace buffet {

template <class Source, class Destination>
void ConvertError(const Source& source,
                  std::unique_ptr<Destination>* destination) {
  const Source* inner_error = source.GetInnerError();
  if (inner_error)
    ConvertError(*inner_error, destination);

  const auto& location = source.GetLocation();
  Destination::AddTo(
      destination,
      base::Location{location.function_name.c_str(), location.file_name.c_str(),
                     location.line_number, base::GetProgramCounter()},
      source.GetDomain(), source.GetCode(), source.GetMessage());
}

}  // namespace buffet

#endif  // BUFFET_WEAVE_ERROR_CONVERSION_H_
