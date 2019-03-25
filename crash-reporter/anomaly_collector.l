/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This flex program reads /var/log/messages as it grows and saves kernel
 * "anomalies" to files. Anomalies can be: kernel warnings, upstart service
 * failures, or any other sufficiently interesting event. It keeps track of
 * anomalies it has seen and reports only the first anomaly of each kind, but
 * maintains a count of all anomalies by using their hashes as buckets in UMA
 * sparse histograms.
 *
 * For example, for kernel warnings each warning is kept track of based on
 * file/line only, ignoring differences in the stack trace.
 *
 * This program also invokes the crash collector, which collects the reported
 * anomalies and prepares them for later shipment to the crash server.
 */

%{

#include "crash-reporter/anomaly_collector.h"

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "metrics/c_metrics_library.h"

static char *cur_service_name;

static char *selinux_audit_text;
static char *selinux_audit_comm;
static char *selinux_audit_name;
static char *selinux_audit_scontext;
static char *selinux_audit_tcontext;
static char *selinux_audit_permission;
static bool selinux_audit_granted;

static int ReportServiceFailure(const char *service_name, const int exit_status);
static int ReportSELinuxViolation(void);
static int KernelWarnStart(void);
static void KernelWarnEnd(void);
static void CollectorInput(char *buf, yy_size_t *result, size_t max_size);
static void SendOomKillSignal(void);

static void AppendToString(char** strp, const char *to_append) {
  if (*strp == NULL) {
    *strp = strdup(to_append);
    return;
  }

  char *new;
  if (asprintf(&new, "%s%s", *strp, to_append) == -1) {
    abort();
  }
  free(*strp);
  *strp = new;
}

static void AppendSELinuxText(void) {
  AppendToString(&selinux_audit_text, yytext);
}

static void CleanUpSELinuxVariables(void) {
  free(selinux_audit_text);
  selinux_audit_text = NULL;
  free(selinux_audit_comm);
  selinux_audit_comm = NULL;
  free(selinux_audit_name);
  selinux_audit_name = NULL;
  free(selinux_audit_scontext);
  selinux_audit_scontext = NULL;
  free(selinux_audit_tcontext);
  selinux_audit_tcontext = NULL;
  free(selinux_audit_permission);
  selinux_audit_permission = NULL;
  selinux_audit_granted = false;
}

#define YY_INPUT(buf, result, max_size) CollectorInput(buf, &result, max_size)

%}

/* Define a few useful regular expressions. */

D               [0-9]
PREFIX          .*" kernel: [ "*{D}+"."{D}+"]"
CUT_HERE        {PREFIX}" ------------[ cut here".*

/* The CPU and PID information got added in the 3.11 kernel development cycle
 * per commit dcb6b45254e2281b6f99ea7f2d51343954aa3ba8. That part is marked
 * optional to make sure the old format still gets accepted. Once we no longer
 * care about kernel version 3.10 and earlier, we can update the code to require
 * CPU and PID to be present unconditionally.
 */
WARNING         {PREFIX}" WARNING:"(" CPU: "{D}+" PID: "{D}+)?" at "
END_TRACE       {PREFIX}" ---[ end trace".*

/* The two meaningful pieces of information are: the name of the process
 * that failed, and its exit status.
 */
UPSTART_WARN {PREFIX}" init: "
SERVICE_FAIL [^ ]+" "[a-z-]+" process ("{D}+") terminated with status "{D}+


SELINUX_AUDIT_PREFIX {PREFIX}" audit: "

/* Use exclusive start conditions. */
%x PRE_WARN WARN SERVICE_FAIL SERVICE_FAIL_SKIP SERVICE_FAIL_EXIT SELINUX_AUDIT

%%
 /* The scanner itself. */

 /* Detect service failures. Retrieve service name and exit status. */
{UPSTART_WARN}/{SERVICE_FAIL}   BEGIN(SERVICE_FAIL);
<SERVICE_FAIL>[^ ]+             {
                                  cur_service_name = strdup(yytext);
                                  BEGIN(SERVICE_FAIL_SKIP);
                                }
<SERVICE_FAIL_SKIP>status       BEGIN(SERVICE_FAIL_EXIT);
<SERVICE_FAIL_EXIT>[^ \n]+      {
                                  ReportServiceFailure(cur_service_name,
                                                       atoi(yytext));
                                  free(cur_service_name);
                                  cur_service_name = NULL;
                                  BEGIN(0);
                                }

 /* Detect kernel warnings. */
^{CUT_HERE}\n{WARNING}          BEGIN(PRE_WARN);

<PRE_WARN>[^ ].*\n              if (KernelWarnStart()) {
                                  /* yytext is
                                   *
                                   * "file:line func+offset/offset() [mod]\n"
                                   *
                                   * The [mod] suffix is only present if the
                                   * address is located within a kernel module.
                                   */
                                  BEGIN(WARN); ECHO;
                                } else {
                                  BEGIN(0);
                                }

 /* Assume the warning ends at the "end trace" line */
<WARN>^{END_TRACE}\n            ECHO; BEGIN(0); KernelWarnEnd();
<WARN>^.*\n                     ECHO;

 /* Detect SELinux violation */
^{SELINUX_AUDIT_PREFIX}         BEGIN(SELINUX_AUDIT);

<SELINUX_AUDIT>audit\([0-9.:]*\): ;  // Skip timestamp
<SELINUX_AUDIT>comm=\"[^"]*\"   {
                                  selinux_audit_comm = strdup(yytext + 6);
                                  // Remove the trailing double-quotes.
                                  selinux_audit_comm[strlen(selinux_audit_comm) - 1] = '\0';
                                  AppendSELinuxText();
                                }
<SELINUX_AUDIT>name=\"[^"]*\"   {
                                  selinux_audit_name = strdup(yytext + 6);
                                  // Remove the trailing double-quotes.
                                  selinux_audit_name[strlen(selinux_audit_name) - 1] = '\0';
                                  AppendSELinuxText();
                                }
<SELINUX_AUDIT>scontext=[^ ]*   {
                                  selinux_audit_scontext = strdup(yytext + 9);
                                  AppendSELinuxText();
                                }
<SELINUX_AUDIT>tcontext=[^ ]*   {
                                  selinux_audit_tcontext = strdup(yytext + 9);
                                  AppendSELinuxText();
                                }
<SELINUX_AUDIT>[{][ ][^ ]*      {
                                  selinux_audit_permission = strdup(yytext + 2);
                                  AppendSELinuxText();
                                }
<SELINUX_AUDIT>avc:[ ]*granted  {
                                  selinux_audit_granted = true;
                                  AppendSELinuxText();
                                }
<SELINUX_AUDIT>\n               {
                                  BEGIN(0);
                                  AppendSELinuxText();
                                  ReportSELinuxViolation();
                                  CleanUpSELinuxVariables();
                                }
<SELINUX_AUDIT>.                AppendSELinuxText();

 /* Detect OOM kill attempt. */
{PREFIX}" Out of memory: Kill process"      SendOomKillSignal();

.|\n                            /* ignore all other input in state 0 */

%%

#define HASH_BITMAP_SIZE        (1 << 15)  /* size in bits */
#define HASH_BITMAP_MASK        (HASH_BITMAP_SIZE - 1)

static const char warn_hist_name[] = "Platform.KernelWarningHashes";
static uint32_t warn_hash_bitmap[HASH_BITMAP_SIZE / 32];
static const char selinux_violation_hist_name[] = "Platform.SELinuxViolationHashes";
static uint32_t selinux_violation_hash_bitmap[HASH_BITMAP_SIZE / 32];
static const char service_failure_hist_name[] = "Platform.ServiceFailureHashes";
static uint32_t service_failure_hash_bitmap[HASH_BITMAP_SIZE / 32];
static CMetricsLibrary metrics_library;

typedef enum {
  kWarningTypeGeneric,
  kWarningTypeWifi,
  kWarningTypeSuspend,
} KernelWarningType;

static int yyin_fd;                    /* instead of FILE *yyin to avoid buffering */
static int i_fd;                       /* for inotify, to detect file changes */
static int testing;                    /* 1 if running test */
static int filter;                     /* 1 when using as filter (for development) */
static int draining;                   /* 1 when draining renamed log file */
static KernelWarningType warn_type;    /* the type of kernel warning */

const char *msg_path = "/var/log/messages";
/* We'll reset this template everytime we use it. */
char dump_path[] = "/tmp/anomaly_collector.------";

static uint32_t StringHash(const char *string) {
  uint32_t hash = 0;
  while (*string != '\0') {
    hash = (hash << 5) + hash + *string++;
  }
  return hash;
}

static char *AlphabetOnly(const char *string) {
  char *alphabet_only = strdup(string);
  char *current = alphabet_only, *next = alphabet_only;
  while (*next != '\0') {
    if (isalpha(*next)) {
      *current++ = *next;
    }
    next++;
  }
  *current = '\0';
  return alphabet_only;
}

static void AppendAlphabetToString(char **dest, char *src) {
  char *alphabet_only = AlphabetOnly(src);
  AppendToString(dest, alphabet_only);
  free(alphabet_only);
}

static uint32_t StringHashAlphabetOnly(const char *string) {
  uint32_t hash = 0;
  while (*string != '\0') {
    if (isalpha(*string))
      hash = (hash << 5) + hash + *string;
    string++;
  }
  return hash;
}

/* We expect only a handful of different anomalies per boot session, so the
 * probability of a collision is very low, and statistically it won't matter
 * (unless anomalies with the same hash also happens in tandem, which is even
 * rarer).
 */
static int HashSeen(const uint32_t *hash_bitmap, uint32_t hash) {
  int word_index = (hash & HASH_BITMAP_MASK) / 32;
  int bit_index = (hash & HASH_BITMAP_MASK) % 32;
  return hash_bitmap[word_index] & 1 << bit_index;
}

static void SetHashSeen(uint32_t *hash_bitmap, uint32_t hash) {
  int word_index = (hash & HASH_BITMAP_MASK) / 32;
  int bit_index = (hash & HASH_BITMAP_MASK) % 32;
  hash_bitmap[word_index] |= 1 << bit_index;
}

static void SendOomKillSignal(void) {
  const char prefix[] = "kernel: [";
  char *start = strstr(yytext, prefix);
  if (start == NULL) {
    errx(1, "lex: fail at input: %s", yytext);
  }
  start += sizeof(prefix) - 1;  // subtract 1 for null byte
  char *endptr;
  long int seconds = strtol(start, &endptr, 10);
  if (endptr == start) {
    errx(1, "lex: no digits at input: %s", start);
  }
  // Skip decimal point.
  start = endptr + 1;
  long int microseconds = strtol(start, &endptr, 10);
  if (endptr == start) {
    errx(1, "lex: no microseconds at input: %s", start);
  }
  int64_t uptime_milliseconds = seconds * 1000 + microseconds / 1000;
  CDBusSendOomSignal(uptime_milliseconds);
}

static int AnomalyStart(const char *histogram_name, uint32_t *hash_bitmap,
                        uint32_t hash) {
  if (!(testing || filter)) {
    CMetricsLibrarySendSparseToUMA(metrics_library, histogram_name, (int) hash);
  }
  if (HashSeen(hash_bitmap, hash))
    return 0;
  SetHashSeen(hash_bitmap, hash);

  /* Reset the template. */
  memset(dump_path + sizeof(dump_path) - 7, 'X', 6);
  int fd = mkostemp(dump_path, O_CLOEXEC);
  if (fd < 0)
    err(1, "mkostemp failed");
  yyout = fdopen(fd, "w");
  if (yyout == NULL)
    err(1, "fdopen failed");
  return 1;
}

static void AnomalyEnd(const char *crash_reporter_flag) {
  fclose(yyout);
  yyout = stdout;               /* for debugging */
  RunCrashReporter(filter, crash_reporter_flag, dump_path);
  unlink(dump_path);
}

static int ReportServiceFailure(const char *service_name, const int exit_status) {
  uint32_t hash;

  hash = StringHash(service_name);
  if (!AnomalyStart(service_failure_hist_name, service_failure_hash_bitmap,
                    hash))
    return 0;

  /* Include exit status in the "stable signature" for crash reports. */
  fprintf(yyout, "%08x-exit%d-%s\n", hash, exit_status, service_name);

  char* arg = NULL;
  if (strncmp("arc-", service_name, 4) == 0) {
    AppendToString(&arg, "--arc_service_failure=");
  } else {
    AppendToString(&arg, "--service_failure=");
  }
  if (service_name != NULL)
    AppendToString(&arg, service_name);
  AnomalyEnd(arg);
  free(arg);
  return 1;
}

static int ReportSELinuxViolation(void) {
  if (selinux_audit_text == NULL)
    return 0;

  uint32_t hash = StringHashAlphabetOnly(selinux_audit_text);

  if (!AnomalyStart(selinux_violation_hist_name, selinux_violation_hash_bitmap,
                    hash))
    return 0;

  char *selinux_violation_sig = NULL;
  if (selinux_audit_granted)
    AppendToString(&selinux_violation_sig, "granted-");
  if (selinux_audit_scontext != NULL)
    AppendToString(&selinux_violation_sig, selinux_audit_scontext);
  AppendToString(&selinux_violation_sig, "-");
  if (selinux_audit_tcontext != NULL)
    AppendToString(&selinux_violation_sig, selinux_audit_tcontext);
  AppendToString(&selinux_violation_sig, "-");
  if (selinux_audit_permission != NULL)
    AppendToString(&selinux_violation_sig, selinux_audit_permission);
  AppendToString(&selinux_violation_sig, "-");
  if (selinux_audit_comm != NULL)
    AppendAlphabetToString(&selinux_violation_sig, selinux_audit_comm);
  AppendToString(&selinux_violation_sig, "-");
  if (selinux_audit_name != NULL)
    AppendAlphabetToString(&selinux_violation_sig, selinux_audit_name);

  if (selinux_violation_sig) {
    fprintf(yyout, "%08x-selinux-%s\n", hash, selinux_violation_sig);
    free(selinux_violation_sig);
  } else
    fprintf(yyout, "%08x-selinux-unknown\n", hash);

  if (selinux_audit_comm != NULL)
    fprintf(yyout, "comm\x01%s\x02", selinux_audit_comm);
  if (selinux_audit_name != NULL)
    fprintf(yyout, "name\x01%s\x02", selinux_audit_name);

  if (selinux_audit_scontext)
    fprintf(yyout, "scontext\x01%s\x02", selinux_audit_scontext);
  if (selinux_audit_tcontext)
    fprintf(yyout, "tcontext\x01%s\x02", selinux_audit_tcontext);

  fputc('\n', yyout);
  fputs(selinux_audit_text, yyout);

  AnomalyEnd("--selinux_violation");

  return 1;
}

static int KernelWarnStart(void) {
  uint32_t hash;
  char *spacep;

  hash = StringHash(yytext);
  if (!AnomalyStart(warn_hist_name, warn_hash_bitmap, hash))
    return 0;

  if (strstr(yytext, "drivers/net/wireless"))
    warn_type = kWarningTypeWifi;
  else if (strstr(yytext, "drivers/idle"))
    warn_type = kWarningTypeSuspend;
  else
    warn_type = kWarningTypeGeneric;

  spacep = index(yytext, ' ');
  if (spacep == NULL || spacep[1] == '\0')
    spacep = " unknown-function";
  fprintf(yyout, "%08x-%s\n", hash, spacep + 1);
  return 1;
}

static void KernelWarnEnd(void) {
  if (warn_type == kWarningTypeWifi)
    AnomalyEnd("--kernel_wifi_warning");
  else if (warn_type == kWarningTypeSuspend)
    AnomalyEnd("--kernel_suspend_warning");
  else
    AnomalyEnd("--kernel_warning");
}

static void CollectorOpenInput(const char *path) {
  yyin_fd = open(path, O_RDONLY);
  if (yyin_fd < 0)
    err(1, "could not open %s", path);

  /* Set up notification of file growth and rename. */
  i_fd = inotify_init();
  if (i_fd < 0)
    err(1, "inotify_init");
  if (inotify_add_watch(i_fd, path, IN_MODIFY | IN_MOVE_SELF) < 0)
    err(1, "inotify_add_watch");
}

/* We replace the default YY_INPUT() for the following reasons:
 *
 * 1.  We want to read data as soon as it becomes available, but the default
 * YY_INPUT() uses buffered I/O.
 *
 * 2.  We want to block on end of input and wait for the file to grow.
 *
 * 3.  We want to detect log rotation, and reopen the input file as needed.
 */
static void CollectorInput(char *buf, yy_size_t *result, size_t max_size) {
  while (1) {
    ssize_t ret = read(yyin_fd, buf, max_size);
    if (ret < 0)
      err(1, "read");
    *result = ret;
    if (*result > 0 || filter)
      return;
    if (draining) {
      /* Assume we're done with this log, and move to next
       * log.  Rsyslogd may keep writing to the old log file
       * for a while, but we don't care since we don't have
       * to be exact.
       */
      close(yyin_fd);
      if (YYSTATE == WARN) {
        /* Be conservative in case we lose the warn
         * terminator during the switch---or we may
         * collect personally identifiable information.
         */
        KernelWarnEnd();
      }
      BEGIN(0);        /* see above comment */
      sleep(1);        /* avoid race with log rotator */
      CollectorOpenInput(msg_path);
      draining = 0;
      continue;
    }
    /* Nothing left to read, so we must wait. */
    struct inotify_event event;
    while (1) {
      int n = read(i_fd, &event, sizeof(event));
      if (n <= 0) {
        if (errno == EINTR)
          continue;
        else
          err(1, "inotify");
      } else
        break;
    }
    if (event.mask & IN_MOVE_SELF) {
      /* The file has been renamed.  Before switching
       * to the new one, we process any remaining
       * content of this file.
       */
      draining = 1;
    }
  }
}

int AnomalyLexer(bool flag_filter, bool flag_test) {
  testing = flag_test;
  filter = flag_filter;

  metrics_library = CMetricsLibraryNew();
  CMetricsLibraryInit(metrics_library);

  /* When filtering with --filter (for development) use stdin for input.
   * Otherwise read input from a file.
   */
  yyin_fd = dup(fileno(stdin));
  if (testing) {
    msg_path = "messages";
  }
  if (!filter) {
    CollectorOpenInput(msg_path);

    /* Go directly to the end of the file.  We don't want to parse the same
     * anomalies multiple times on reboot/restart.  We might miss some
     * anomalies, but so be it---it's too hard to keep track reliably of the
     * last parsed position in the syslog.
     */
    if (lseek(yyin_fd, 0, SEEK_END) < 0)
      err(1, "could not lseek %s", msg_path);
  }

  stdin = freopen("/dev/null", "r", stdin);

  /* Go! */
  return yylex();
}

/* Flex should really know not to generate these functions.
 */
void UnusedFunctionWarningSuppressor(void) {
  yyunput(0, 0);
  (void) input();
}
