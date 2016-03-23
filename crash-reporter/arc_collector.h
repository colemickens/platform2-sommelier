// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_ARC_COLLECTOR_H_
#define CRASH_REPORTER_ARC_COLLECTOR_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "crash-reporter/user_collector_base.h"

// Collector for system crashes in the ARC container.
class ArcCollector : public UserCollectorBase {
 public:
  static const uid_t kSystemUser = 1000;

  struct Context {
    virtual ~Context() = default;

    virtual bool GetArcPid(pid_t *pid) const = 0;
    virtual bool GetPidNamespace(pid_t pid, std::string *ns) const = 0;
    virtual bool GetExeBaseName(pid_t pid, std::string *exe) const = 0;
    virtual bool GetCommand(pid_t pid, std::string *command) const = 0;
  };

  using ContextPtr = std::unique_ptr<Context>;

  ArcCollector() = default;
  explicit ArcCollector(ContextPtr context);
  ~ArcCollector() override = default;

  const Context &context() const { return *context_; }

  // Returns false if the query failed, which may happen during teardown of the
  // ARC container. Since the behavior of user collectors is determined by
  // IsArcProcess, there is a (rare) race condition for crashes that occur
  // during teardown.
  bool IsArcProcess(pid_t pid) const;

  static bool IsArcRunning();

 private:
  FRIEND_TEST(ArcCollectorTest, GetExeBaseNameForUserCrash);
  FRIEND_TEST(ArcCollectorTest, GetExeBaseNameForArcCrash);
  FRIEND_TEST(ArcCollectorTest, ShouldDump);

  class ArcContext : public Context {
   public:
    explicit ArcContext(ArcCollector *collector)
        : collector_(collector) {}

    bool GetArcPid(pid_t *pid) const override;
    bool GetPidNamespace(pid_t pid, std::string *ns) const override;
    bool GetExeBaseName(pid_t pid, std::string *exe) const override;
    bool GetCommand(pid_t pid, std::string *command) const override;

   private:
    ArcCollector *const collector_;
  };

  // CrashCollector overrides.
  bool GetExecutableBaseNameFromPid(pid_t pid, std::string *base_name) override;

  // UserCollectorBase overrides.
  bool ShouldDump(pid_t pid,
                  uid_t uid,
                  const std::string &exec,
                  std::string *reason) override;

  ErrorType ConvertCoreToMinidump(pid_t pid,
                                  const base::FilePath &container_dir,
                                  const base::FilePath &core_path,
                                  const base::FilePath &minidump_path) override;

  // Adds the crash |type| and Chrome version as metadata. The |add_arc_version|
  // option requires privilege to access the ARC root.
  void AddArcMetaData(const std::string &type, bool add_arc_version);

  const ContextPtr context_{new ArcContext(this)};

  DISALLOW_COPY_AND_ASSIGN(ArcCollector);
};

#endif  // CRASH_REPORTER_ARC_COLLECTOR_H_
