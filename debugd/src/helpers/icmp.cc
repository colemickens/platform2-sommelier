// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ICMP helper - emits info about ICMP connectivity to a specified host as json.
// Example output:
// { "4.2.2.1":
//     { "sent": 4,
//       "recvd": 4,
//       "time": 3005,
//       "min": 5.789000,
//       "avg": 5.913000,
//       "max": 6.227000,
//       "dev": 0.197000
//     }
// }
// All times are in milliseconds. "time" is the total time taken by ping(1).


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *why) {
  printf("<%s>\n", why);
  exit(1);
}

static int is_ipaddr(const char *maybe) {
  // Allow v4 or v6 addresses
  const char *allowed = "ABCDEFabcdef0123456789.:";
  while (*maybe)
    if (!strchr(allowed, *maybe++))
      return 0;
  return 1;
}

int main(int argc, char *argv[]) {
  char cmdbuf[1024];
  char outbuf[1024];
  FILE *out;
  int sent, recvd, loss, time;
  float min = 0.0, avg = 0.0, max = 0.0, mdev = 0.0;
  int seen = 0;

  if (argc != 2)
    die("wrong number of args");
  if (!is_ipaddr(argv[1]))
    die("not ip address");
  // Yeah. User input into a buffer that we then pass to /bin/sh. Time to be
  // careful.
  snprintf(cmdbuf, sizeof(cmdbuf), "/bin/ping -c 4 -w 10 -nq %s", argv[1]);
  out = popen(cmdbuf, "r");
  if (!out)
    die("can't create subprocess");
  while (fgets(outbuf, sizeof(outbuf), out)) {
    if (sscanf(outbuf,
               "%d packets transmitted, %d received, %d%% packet loss, time %dms",
               &sent, &recvd, &loss, &time) == 4)
      seen++;
    if (sscanf(outbuf,
               "rtt min/avg/max/mdev = %f/%f/%f/%f ms",
               &min, &avg, &max, &mdev) == 4)
      seen++;
  }
  pclose(out);
  if (seen != 2)
    die("didn't get all output");
  printf("{ \"%s\":\n", argv[1]);
  printf("    { \"sent\": %d,\n", sent);
  printf("      \"recvd\": %d,\n", recvd);
  printf("      \"time\": %d,\n", time);
  printf("      \"min\": %f,\n", min);
  printf("      \"avg\": %f,\n", avg);
  printf("      \"max\": %f,\n", max);
  printf("      \"dev\": %f\n", mdev);
  printf("    }\n");
  printf("}\n");
  return 0;
}
