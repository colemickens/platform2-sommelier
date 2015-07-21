// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_
#define LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_

#include <base/values.h>
#include <chromeos/errors/error.h>

namespace weave {

class Command {
 public:
  // This interface lets the command to notify clients about changes.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnResultsChanged() = 0;
    virtual void OnStatusChanged() = 0;
    virtual void OnProgressChanged() = 0;
    virtual void OnCommandDestroyed() = 0;
  };

  // Returns JSON representation of the command.
  virtual std::unique_ptr<base::DictionaryValue> ToJson() const = 0;

  // Adds an observer for this command. The observer object is not owned by this
  // class.
  virtual void AddObserver(Observer* observer) = 0;

 protected:
  virtual ~Command() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_
