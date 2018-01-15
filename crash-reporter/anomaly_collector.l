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
#include <fcntl.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdarg.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "metrics/c_metrics_library.h"

char *cur_service_name;

int ReportServiceFailure(const char *service_name, const int exit_status);
int KernelWarnStart(void);
void KernelWarnEnd(void);
void CollectorInput(char *buf, yy_size_t *result, size_t max_size);

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
SERVICE_FAIL [^ ]+" main process ("{D}+") terminated with status "{D}+

/* Use exclusive start conditions. */
%x PRE_WARN WARN SERVICE_FAIL SERVICE_FAIL_SKIP SERVICE_FAIL_EXIT

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
.|\n                            /* ignore all other input in state 0 */
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

%%

#define HASH_BITMAP_SIZE        (1 << 15)  /* size in bits */
#define HASH_BITMAP_MASK        (HASH_BITMAP_SIZE - 1)

const char warn_hist_name[] = "Platform.KernelWarningHashes";
uint32_t warn_hash_bitmap[HASH_BITMAP_SIZE / 32];
const char service_failure_hist_name[] = "Platform.ServiceFailureHashes";
uint32_t service_failure_hash_bitmap[HASH_BITMAP_SIZE / 32];
CMetricsLibrary metrics_library;

typedef enum {
  kWarningTypeGeneric,
  kWarningTypeWifi,
  kWarningTypeSuspend,
} KernelWarningType;

const char *prog_name;          /* the name of this program */
int yyin_fd;                    /* instead of FILE *yyin to avoid buffering */
int i_fd;                       /* for inotify, to detect file changes */
int testing;                    /* 1 if running test */
int filter;                     /* 1 when using as filter (for development) */
int fifo;                       /* 1 when reading from fifo (for devel) */
int draining;                   /* 1 when draining renamed log file */
KernelWarningType warn_type;    /* the type of kernel warning */

const char *msg_path = "/var/log/messages";
const char anomaly_dump_dir[]  = "/run/anomaly-collector";
const char *warn_dump_path = "/run/anomaly-collector/warning";
const char *warn_crash_reporter_command;
const char *wifi_warn_crash_reporter_command;
const char *suspend_warn_crash_reporter_command;
const char *service_fail_dump_path = "/run/anomaly-collector/service-fail";
const char *service_fail_crash_reporter_command;

__attribute__((__format__(__printf__, 1, 2)))
static void Die(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s: ", prog_name);
  vfprintf(stderr, format, ap);
  exit(1);
}

static void RunCrashReporter(const char *crash_reporter_command) {
  int status = system(crash_reporter_command);
  if (status != 0)
    Die("%s exited with status %d\n", crash_reporter_command, status);
}

static uint32_t StringHash(const char *string) {
  uint32_t hash = 0;
  while (*string != '\0') {
    hash = (hash << 5) + hash + *string++;
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

static int AnomalyStart(const char *dump_path, const char *histogram_name,
                        uint32_t *hash_bitmap, uint32_t hash) {
  if (!(testing || fifo || filter)) {
    CMetricsLibrarySendSparseToUMA(metrics_library, histogram_name, (int) hash);
  }
  if (HashSeen(hash_bitmap, hash))
    return 0;
  SetHashSeen(hash_bitmap, hash);

  yyout = fopen(dump_path, "w");
  if (yyout == NULL)
    Die("fopen %s failed: %s\n", dump_path, strerror(errno));
  return 1;
}

static void AnomalyEnd(const char *crash_reporter_command) {
  if (filter)
    return;
  fclose(yyout);
  yyout = stdout;               /* for debugging */
  RunCrashReporter(crash_reporter_command);
}

int ReportServiceFailure(const char *service_name, const int exit_status) {
  uint32_t hash;

  if (filter)
    return 1;

  hash = StringHash(service_name);
  if (!AnomalyStart(service_fail_dump_path, service_failure_hist_name,
                    service_failure_hash_bitmap, hash))
    return 0;

  /* Include exit status in the "stable signature" for crash reports. */
  fprintf(yyout, "%08x-exit%d-%s\n", hash, exit_status, service_name);

  AnomalyEnd(service_fail_crash_reporter_command);
  return 1;
}

int KernelWarnStart(void) {
  uint32_t hash;
  char *spacep;

  if (filter)
    return 1;

  hash = StringHash(yytext);
  if (!AnomalyStart(warn_dump_path, warn_hist_name,
                    warn_hash_bitmap, hash))
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

void KernelWarnEnd(void) {
  if (warn_type == kWarningTypeWifi)
    AnomalyEnd(wifi_warn_crash_reporter_command);
  else if (warn_type == kWarningTypeSuspend)
    AnomalyEnd(suspend_warn_crash_reporter_command);
  else
    AnomalyEnd(warn_crash_reporter_command);
}

static void CollectorOpenInput(const char *path) {
  yyin_fd = open(path, O_RDONLY);
  if (yyin_fd < 0)
    Die("could not open %s: %s\n", path, strerror(errno));

  /* Set up notification of file growth and rename. */
  i_fd = inotify_init();
  if (i_fd < 0)
    Die("inotify_init: %s\n", strerror(errno));
  if (inotify_add_watch(i_fd, path, IN_MODIFY | IN_MOVE_SELF) < 0)
    Die("inotify_add_watch: %s\n", strerror(errno));
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
void CollectorInput(char *buf, yy_size_t *result, size_t max_size) {
  while (1) {
    ssize_t ret = read(yyin_fd, buf, max_size);
    if (ret < 0)
      Die("read: %s", strerror(errno));
    *result = ret;
    if (*result > 0 || fifo || filter)
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
          Die("inotify: %s\n", strerror(errno));
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

int main(int argc, char **argv) {
  int result;
  struct passwd *user;
  prog_name = argv[0];

  if (argc == 2 && strcmp(argv[1], "--test") == 0)
    testing = 1;
  else if (argc == 2 && strcmp(argv[1], "--filter") == 0)
    filter = 1;
  else if (argc == 2 && strcmp(argv[1], "--fifo") == 0) {
    fifo = 1;
  } else if (argc != 1) {
    fprintf(stderr,
            "usage: %s [single-flag]\n"
            "flags (for testing only):\n"
            "--fifo\tinput is fifo \"fifo\", output is stdout\n"
            "--filter\tinput is stdin, output is stdout\n"
            "--test\trun self-test\n",
            prog_name);
    exit(1);
  }

  metrics_library = CMetricsLibraryNew();
  CMetricsLibraryInit(metrics_library);

  warn_crash_reporter_command = testing ?
    "./anomaly_collector_test_reporter.sh warning" :
    "/sbin/crash_reporter --kernel_warning";

  wifi_warn_crash_reporter_command = testing ?
    "./anomaly_collector_test_reporter.sh warning wifi-warning" :
    "/sbin/crash_reporter --kernel_wifi_warning";

  suspend_warn_crash_reporter_command = testing ?
    "./anomaly_collector_test_reporter.sh warning suspend-warning" :
    "/sbin/crash_reporter --kernel_suspend_warning";

  service_fail_crash_reporter_command = testing ?
    "./anomaly_collector_test_reporter.sh service-fail" :
    "/sbin/crash_reporter --service_failure";

  /* When filtering with --filter (for development) use stdin for input.
   * Otherwise read input from a file or a fifo.
   */
  yyin_fd = fileno(stdin);
  if (testing) {
    msg_path = "messages";
    warn_dump_path = "warning";
    service_fail_dump_path = "service-fail";
  }
  if (fifo) {
    msg_path = "fifo";
  }
  if (!filter) {
    CollectorOpenInput(msg_path);

    if (!fifo) {
      /* Go directly to the end of the file.  We don't want to parse the same
       * anomalies multiple times on reboot/restart.  We might miss some
       * anomalies, but so be it---it's too hard to keep track reliably of the
       * last parsed position in the syslog.
       */
      if (lseek(yyin_fd, 0, SEEK_END) < 0)
        Die("could not lseek %s: %s\n", msg_path, strerror(errno));
    }
  }

  /* Create directory for dump file.  Still need to be root here. */
  unlink(warn_dump_path);
  unlink(service_fail_dump_path);
  if (!testing && !fifo && !filter) {
    rmdir(anomaly_dump_dir);
    result = mkdir(anomaly_dump_dir, 0755);
    if (result < 0)
      Die("could not create %s: %s\n",
          anomaly_dump_dir, strerror(errno));
  }

  if (0) {
    /* TODO(semenzato): put this back in once we decide it's safe
     * to make /var/spool/crash rwxrwxrwx root, or use a different
     * owner and setuid for the crash reporter as well.
     */

    /* Get low privilege uid, gid. */
    user = getpwnam("chronos");
    if (user == NULL)
      Die("getpwnam failed\n");

    /* Change dump directory ownership. */
    if (chown(anomaly_dump_dir, user->pw_uid, user->pw_gid) < 0)
      Die("chown: %s\n", strerror(errno));

    /* Drop privileges. */
    if (setuid(user->pw_uid) < 0) {
      Die("setuid: %s\n", strerror(errno));
    }
  }

  /* Go! */
  return yylex();
}

/* Flex should really know not to generate these functions.
 */
void UnusedFunctionWarningSuppressor(void) {
  yyunput(0, 0);
  (void) input();
}
