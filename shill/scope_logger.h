// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SCOPE_LOGGER_H_
#define SHILL_SCOPE_LOGGER_H_

#include <bitset>
#include <string>

#include <base/basictypes.h>
#include <base/lazy_instance.h>
#include <base/logging.h>
#include <gtest/gtest_prod.h>

// Helper macros to enable logging based on scope and verbose level.
//
// The SLOG macro and its variants are similar to the LOG and VLOG macros
// defined in base/logging.h, except that the SLOG macros take an additional
// |scope| argument to enable logging only if |scope| is enabled.
//
// Like VLOG, SLOG macros internally map verbosity to LOG severity using
// negative values, i.e. SLOG(scope, 1) corresponds to LOG(-1).
//
// Example usages:
//  SLOG(Service, 1) << "Printed when the 'service' scope is enabled and "
//                      "the verbose level is greater than or equal to 1";
//
//  SLOG_IF(Service, 1, (size > 1024))
//      << "Printed when the 'service' scope is enabled, the verbose level "
//         "is greater than or equal to 1, and size is more than 1024";
//
//  SPLOG and SPLOG_IF are similar to SLOG and SLOG_IF, respectively, but
//  append the last system error to the message in string form, which is
//  taken from errno.

#define SLOG_IS_ON(scope, verbose_level) \
  ::shill::ScopeLogger::GetInstance()->IsLogEnabled( \
      ::shill::ScopeLogger::k##scope, -verbose_level)

#define SLOG_STREAM(verbose_level) \
  ::logging::LogMessage(__FILE__, __LINE__, -verbose_level).stream()

#define SLOG(scope, verbose_level) \
  LAZY_STREAM(SLOG_STREAM(verbose_level), SLOG_IS_ON(scope, verbose_level))

#define SLOG_IF(scope, verbose_level, condition) \
  LAZY_STREAM(SLOG_STREAM(verbose_level), \
              SLOG_IS_ON(scope, verbose_level) && (condition))

#define SPLOG_STREAM(verbose_level) \
  ::logging::ErrnoLogMessage(__FILE__, __LINE__, -verbose_level, \
                             ::logging::GetLastSystemErrorCode()).stream()

#define SPLOG(scope, verbose_level) \
  LAZY_STREAM(SPLOG_STREAM(verbose_level), SLOG_IS_ON(scope, verbose_level))

#define SPLOG_IF(scope, verbose_level, condition) \
  LAZY_STREAM(SPLOG_STREAM(verbose_level), \
              SLOG_IS_ON(scope, verbose_level) && (condition))

namespace shill {

// A class that enables logging based on scope and verbose level. It is not
// intended to be used directly but via the SLOG() macros.
class ScopeLogger {
 public:
  // Logging scopes.
  //
  // Update kScopeNames in scope_logger.cc after changing this enumerated type.
  // These scope identifiers are sorted by their scope names alphabetically.
  enum Scope {
    kCellular = 0,
    kConnection,
    kCrypto,
    kDaemon,
    kDBus,
    kDevice,
    kDHCP,
    kDNS,
    kEthernet,
    kHTTP,
    kHTTPProxy,
    kInet,
    kManager,
    kMetrics,
    kModem,
    kPortal,
    kPower,
    kProfile,
    kProperty,
    kResolver,
    kRoute,
    kRTNL,
    kService,
    kStorage,
    kTask,
    kVPN,
    kWiFi,
    kWiMax,
    kNumScopes
  };

  // Returns a singleton of this class.
  static ScopeLogger *GetInstance();

  ~ScopeLogger();

  // Returns true if logging is enabled for |scope| and |verbose_level|, i.e.
  // scope_enable_[|scope|] is true and |verbose_level| <= |verbose_level_|
  bool IsLogEnabled(Scope scope, int verbose_level) const;

  // Returns a string comprising the names, separated by commas, of all scopes.
  std::string GetAllScopeNames() const;

  // Returns a string comprising the names, separated by plus signs, of all
  // scopes that are enabled for logging.
  std::string GetEnabledScopeNames() const;

  // Enables/disables scopes as specified by |expression|.
  //
  // |expression| is a string comprising a sequence of scope names, each
  // prefixed by a plus '+' or minus '-' sign. A scope prefixed by a plus
  // sign is enabled for logging, whereas a scope prefixed by a minus sign
  // is disabled for logging. Scopes that are not mentioned in |expression|
  // remain the same state.
  //
  // To allow resetting the state of all scopes, an exception is made for the
  // first scope name in the sequence, which may not be prefixed by any sign.
  // That is considered as an implicit plus sign for that scope and also
  // indicates that all scopes are first disabled before enabled by
  // |expression|.
  //
  // If |expression| is an empty string, all scopes are disabled. Any unknown
  // scope name found in |expression| is ignored.
  void EnableScopesByName(const std::string &expression);

  // Sets the verbose level for all scopes to |verbose_level|.
  void set_verbose_level(int verbose_level) { verbose_level_ = verbose_level; }

 private:
  // Required for constructing LazyInstance<ScopeLogger>.
  friend struct base::DefaultLazyInstanceTraits<ScopeLogger>;
  friend class ScopeLoggerTest;
  FRIEND_TEST(ScopeLoggerTest, GetEnabledScopeNames);
  FRIEND_TEST(ScopeLoggerTest, SetScopeEnabled);
  FRIEND_TEST(ScopeLoggerTest, SetVerboseLevel);

  // Disables logging for all scopes.
  void DisableAllScopes();

  // Enables or disables logging for |scope|.
  void SetScopeEnabled(Scope scope, bool enabled);

  // Boolean values to indicate whether logging is enabled for each scope.
  std::bitset<kNumScopes> scope_enabled_;

  // Verbose level that is applied to all scopes.
  int verbose_level_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeLogger);
};

}  // namespace shill

#endif  // SHILL_SCOPE_LOGGER_H_
