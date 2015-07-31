// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_
#define LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_

#include <string>

#include <base/values.h>
#include <chromeos/errors/error.h>

namespace weave {

enum class CommandStatus {
  kQueued,
  kInProgress,
  kPaused,
  kError,
  kDone,
  kCancelled,
  kAborted,
  kExpired,
};

enum class CommandOrigin { kLocal, kCloud };

class Command {
 public:
  // This interface lets the command to notify clients about changes.
  class Observer {
   public:
    virtual void OnResultsChanged() = 0;
    virtual void OnStatusChanged() = 0;
    virtual void OnProgressChanged() = 0;
    virtual void OnCommandDestroyed() = 0;

   protected:
    virtual ~Observer() = default;
  };

  // Adds an observer for this command. The observer object is not owned by this
  // class.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer for this command.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the full command ID.
  virtual const std::string& GetID() const = 0;

  // Returns the full name of the command.
  virtual const std::string& GetName() const = 0;

  // Returns the command category.
  virtual const std::string& GetCategory() const = 0;

  // Returns the command status.
  virtual CommandStatus GetStatus() const = 0;

  // Returns the origin of the command.
  virtual CommandOrigin GetOrigin() const = 0;

  // Returns the command parameters.
  virtual std::unique_ptr<base::DictionaryValue> GetParameters() const = 0;

  // Returns the command progress.
  virtual std::unique_ptr<base::DictionaryValue> GetProgress() const = 0;

  // Returns the command results.
  virtual std::unique_ptr<base::DictionaryValue> GetResults() const = 0;

  // Updates the command progress. The |progress| should match the schema.
  // Returns false if |progress| value is incorrect.
  virtual bool SetProgress(const base::DictionaryValue& progress,
                           chromeos::ErrorPtr* error) = 0;

  // Updates the command results. The |results| should match the schema.
  // Returns false if |results| value is incorrect.
  virtual bool SetResults(const base::DictionaryValue& results,
                          chromeos::ErrorPtr* error) = 0;

  // Aborts command execution.
  virtual void Abort() = 0;

  // Cancels command execution.
  virtual void Cancel() = 0;

  // Marks the command as completed successfully.
  virtual void Done() = 0;

  // Returns JSON representation of the command.
  virtual std::unique_ptr<base::DictionaryValue> ToJson() const = 0;

 protected:
  virtual ~Command() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_COMMAND_H_
