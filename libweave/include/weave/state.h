// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_STATE_H_
#define LIBWEAVE_INCLUDE_WEAVE_STATE_H_

#include <base/callback.h>
#include <base/values.h>
#include <chromeos/variant_dictionary.h>

namespace weave {

class State {
 public:
  // Sets callback which is called when stat is changed.
  virtual void AddOnChangedCallback(const base::Closure& callback) = 0;

  // Updates a multiple property values.
  virtual bool SetProperties(const chromeos::VariantDictionary& property_set,
                             chromeos::ErrorPtr* error) = 0;

  // Returns aggregated state properties across all registered packages as
  // a JSON object that can be used to send the device state to the GCD server.
  virtual std::unique_ptr<base::DictionaryValue> GetStateValuesAsJson()
      const = 0;

 protected:
  virtual ~State() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_STATE_H_
