// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/krb5_interface.h"

#include <utility>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <krb5.h>

namespace kerberos {

namespace {

// Environment variable for the Kerberos configuration (krb5.conf).
constexpr char kKrb5ConfigEnvVar[] = "KRB5_CONFIG";

enum class Action { AcquireTgt, RenewTgt };

struct Options {
  std::string principal_name;
  std::string password;
  std::string krb5cc_path;
  std::string config_path;
  Action action = Action::AcquireTgt;
};

// Encapsulates krb5 context data required for kinit.
class KinitContext {
 public:
  explicit KinitContext(Options options) : options_(std::move(options)) {
    memset(&k5_, 0, sizeof(k5_));
  }

  // Runs kinit with the options passed to the constructor. Only call once per
  // context. While in principle it should be fine to run multiple times, the
  // code path probably hasn't been tested (kinit does not call this multiple
  // times).
  ErrorType Run() {
    DCHECK(!did_run_);
    did_run_ = true;

    ErrorType error = Initialize();
    if (error == ERROR_NONE)
      error = RunKinit();
    Finalize();
    return error;
  }

 private:
  // The following code has been adapted from kinit.c in the mit-krb5 code base.
  // It has been formatted to fit this screen.

  struct Krb5Data {
    krb5_context ctx;
    krb5_ccache out_cc;
    krb5_principal me;
    char* name;
  };

  // Wrapper around krb5 data to get rid of the gotos in the original code.
  struct KInitData {
    // Pointer to parent data, not owned.
    const Krb5Data* k5 = nullptr;
    krb5_creds my_creds;
    krb5_get_init_creds_opt* options = nullptr;

    // The lifetime of the |k5| pointer must exceed the lifetime of this object.
    explicit KInitData(const Krb5Data* k5) : k5(k5) {
      memset(&my_creds, 0, sizeof(my_creds));
    }

    ~KInitData() {
      if (options)
        krb5_get_init_creds_opt_free(k5->ctx, options);
      if (my_creds.client == k5->me)
        my_creds.client = nullptr;
      krb5_free_cred_contents(k5->ctx, &my_creds);
    }
  };

  // Maps some common krb5 error codes to our internal codes. If something is
  // not reported properly, add more cases here.
  ErrorType TranslateErrorCode(errcode_t code) {
    switch (code) {
      case KRB5KDC_ERR_NONE:
        return ERROR_NONE;

      case KRB5_KDC_UNREACH:
        return ERROR_NETWORK_PROBLEM;

      case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
        return ERROR_BAD_PRINCIPAL;

      case KRB5KRB_AP_ERR_BAD_INTEGRITY:
      case KRB5KDC_ERR_PREAUTH_FAILED:
        return ERROR_BAD_PASSWORD;

      case KRB5KDC_ERR_KEY_EXP:
        return ERROR_PASSWORD_EXPIRED;

      // TODO(ljusten): Verify
      case KRB5_KPASSWD_SOFTERROR:
        return ERROR_PASSWORD_REJECTED;

      // TODO(ljusten): Verify
      case KRB5_FCC_NOFILE:
        return ERROR_NO_CREDENTIALS_CACHE_FOUND;

      // TODO(ljusten): Verify
      case KRB5KRB_AP_ERR_TKT_EXPIRED:
        return ERROR_KERBEROS_TICKET_EXPIRED;

      case KRB5KDC_ERR_ETYPE_NOSUPP:
        return ERROR_KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE;

      case KRB5_REALM_UNKNOWN:
        return ERROR_CONTACTING_KDC_FAILED;

      default:
        return ERROR_UNKNOWN_KRB5_ERROR;
    }
  }

  // Converts the krb5 |code| to a human readable error message.
  std::string GetErrorMessage(errcode_t code) {
    const char* emsg = krb5_get_error_message(k5_.ctx, code);
    std::string msg = base::StringPrintf("%s (%ld)", emsg, code);
    krb5_free_error_message(k5_.ctx, emsg);
    return msg;
  }

  // Initializes krb5 data.
  ErrorType Initialize() {
    krb5_error_code ret = krb5_init_context(&k5_.ctx);
    if (ret) {
      LOG(ERROR) << GetErrorMessage(ret) << " while initializing context";
      return TranslateErrorCode(ret);
    }

    ret = krb5_cc_resolve(k5_.ctx, options_.krb5cc_path.c_str(), &k5_.out_cc);
    if (ret) {
      LOG(ERROR) << GetErrorMessage(ret) << " resolving ccache";
      return TranslateErrorCode(ret);
    }

    ret = krb5_parse_name_flags(k5_.ctx, options_.principal_name.c_str(),
                                0 /* flags */, &k5_.me);
    if (ret) {
      LOG(ERROR) << GetErrorMessage(ret) << " when parsing name";
      return TranslateErrorCode(ret);
    }

    ret = krb5_unparse_name(k5_.ctx, k5_.me, &k5_.name);
    if (ret) {
      LOG(ERROR) << GetErrorMessage(ret) << " when unparsing name";
      return TranslateErrorCode(ret);
    }

    options_.principal_name = k5_.name;
    return ERROR_NONE;
  }

  // Finalizes krb5 data.
  void Finalize() {
    krb5_free_unparsed_name(k5_.ctx, k5_.name);
    krb5_free_principal(k5_.ctx, k5_.me);
    if (k5_.out_cc != nullptr)
      krb5_cc_close(k5_.ctx, k5_.out_cc);
    krb5_free_context(k5_.ctx);
    memset(&k5_, 0, sizeof(k5_));
  }

  // Runs the actual kinit code and acquires/renews tickets.
  ErrorType RunKinit() {
    krb5_error_code ret;
    KInitData d(&k5_);

    ret = krb5_get_init_creds_opt_alloc(k5_.ctx, &d.options);
    if (ret) {
      LOG(ERROR) << GetErrorMessage(ret) << "while getting options";
      return TranslateErrorCode(ret);
    }

    ret =
        krb5_get_init_creds_opt_set_out_ccache(k5_.ctx, d.options, k5_.out_cc);
    if (ret) {
      LOG(ERROR) << GetErrorMessage(ret) << "while getting options";
      return TranslateErrorCode(ret);
    }

    // To get notified of expiry, see
    // krb5_get_init_creds_opt_set_expire_callback

    switch (options_.action) {
      case Action::AcquireTgt:
        ret = krb5_get_init_creds_password(
            k5_.ctx, &d.my_creds, k5_.me, options_.password.c_str(),
            nullptr /* prompter */, nullptr /* data */, 0 /* start_time */,
            nullptr /* in_tkt_service */, d.options);
        break;
      case Action::RenewTgt:
        ret = krb5_get_renewed_creds(k5_.ctx, &d.my_creds, k5_.me, k5_.out_cc,
                                     nullptr /* options_.in_tkt_service */);
        break;
    }

    if (ret) {
      LOG(ERROR) << GetErrorMessage(ret);
      return TranslateErrorCode(ret);
    }

    if (options_.action != Action::AcquireTgt) {
      ret = krb5_cc_initialize(k5_.ctx, k5_.out_cc, k5_.me);
      if (ret) {
        LOG(ERROR) << GetErrorMessage(ret) << " when initializing cache";
        return TranslateErrorCode(ret);
      }

      ret = krb5_cc_store_cred(k5_.ctx, k5_.out_cc, &d.my_creds);
      if (ret) {
        LOG(ERROR) << GetErrorMessage(ret) << " while storing credentials";
        return TranslateErrorCode(ret);
      }
    }

    return ERROR_NONE;
  }

  Krb5Data k5_;
  Options options_;
  bool did_run_ = false;
};

}  // namespace

Krb5Interface::Krb5Interface() = default;

Krb5Interface::~Krb5Interface() = default;

ErrorType Krb5Interface::AcquireTgt(const std::string& principal_name,
                                    const std::string& password,
                                    const std::string& krb5cc_path,
                                    const std::string& krb5conf_path) {
  Options options;
  options.action = Action::AcquireTgt;
  options.principal_name = principal_name;
  options.password = password;
  options.krb5cc_path = krb5cc_path;
  setenv(kKrb5ConfigEnvVar, krb5conf_path.c_str(), 1);
  ErrorType error = KinitContext(std::move(options)).Run();
  unsetenv(kKrb5ConfigEnvVar);
  return error;
}

ErrorType Krb5Interface::RenewTgt(const std::string& principal_name,
                                  const std::string& krb5cc_path,
                                  const std::string& config_path) {
  Options options;
  options.action = Action::RenewTgt;
  options.principal_name = principal_name;
  options.krb5cc_path = krb5cc_path;
  setenv(kKrb5ConfigEnvVar, config_path.c_str(), 1);
  ErrorType error = KinitContext(std::move(options)).Run();
  unsetenv(kKrb5ConfigEnvVar);
  return error;
}

}  // namespace kerberos
