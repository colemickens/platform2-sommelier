// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DPSL_INTERNAL_CALLBACK_UTILS_H_
#define DIAGNOSTICS_DPSL_INTERNAL_CALLBACK_UTILS_H_

#include <functional>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/location.h>
#include <base/memory/ref_counted.h>
#include <base/task_runner.h>
#include <base/threading/thread_task_runner_handle.h>

namespace diagnostics {

// Several versions of the function that transforms base::Callback into
// std::function:

template <typename ReturnType>
inline std::function<ReturnType()> MakeStdFunctionFromCallback(
    base::Callback<ReturnType()> callback) {
  return std::bind(
      [](base::Callback<ReturnType()> callback) { return callback.Run(); },
      std::move(callback));
}

template <typename ReturnType, typename Arg1Type>
inline std::function<ReturnType(Arg1Type)> MakeStdFunctionFromCallback(
    base::Callback<ReturnType(Arg1Type)> callback) {
  return std::bind(
      [](base::Callback<ReturnType(Arg1Type)> callback, Arg1Type&& arg1) {
        return callback.Run(std::forward<Arg1Type>(arg1));
      },
      std::move(callback), std::placeholders::_1);
}

namespace internal {

template <typename ReturnType, typename... ArgTypes>
inline ReturnType RunStdFunctionWithArgs(
    std::function<ReturnType(ArgTypes...)> function, ArgTypes... args) {
  return function(std::forward<ArgTypes>(args)...);
}

}  // namespace internal

// Transforms std::function into base::Callback.
template <typename ReturnType, typename... ArgTypes>
inline base::Callback<ReturnType(ArgTypes...)> MakeCallbackFromStdFunction(
    std::function<ReturnType(ArgTypes...)> function) {
  return base::Bind(
      &internal::RunStdFunctionWithArgs<ReturnType, ArgTypes...>,
      base::Passed(std::move(function)));
}

namespace internal {

template <typename... ArgTypes>
inline void RunCallbackOnTaskRunner(scoped_refptr<base::TaskRunner> task_runner,
                                    const base::Location& location,
                                    base::Callback<void(ArgTypes...)> callback,
                                    ArgTypes... args) {
  task_runner->PostTask(location, base::Bind(std::move(callback),
                                             base::Passed(std::move(args))...));
}

}  // namespace internal

// Returns a callback that remembers the current task runner and, when called,
// posts |callback| to it (with all arguments forwarded).
template <typename... ArgTypes>
inline base::Callback<void(ArgTypes...)> MakeOriginTaskRunnerPostingCallback(
    const base::Location& location,
    base::Callback<void(ArgTypes...)> callback) {
  return base::Bind(&internal::RunCallbackOnTaskRunner<ArgTypes...>,
                    base::ThreadTaskRunnerHandle::Get(), location,
                    std::move(callback));
}

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DPSL_INTERNAL_CALLBACK_UTILS_H_
