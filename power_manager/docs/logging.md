# Chrome OS Power Manager Logging

## Locations

powerd writes logs to the `/var/log/power_manager` directory. It creates a new
file every time it starts and updates `powerd.LATEST` and `powerd.PREVIOUS`
symlinks to point to the current and previous files.

Any output written to `stdout` or `stderr` (including messages that are printed
before the logging library has been initialized) are redirected to
`/var/log/powerd.out`.

Output from the `powerd_suspend` script, along with suspend- and resume-related
kernel messages, are written to `/var/log/messages`.

## Guidelines

powerd receives input (e.g. user/video/audio activity, lid events, etc.) and
performs actions sporadically; thirty-second intervals where nothing happens are
common for an idle system. Having logs of these events is essential to
reconstruct the past when investigating user feedback reports.

powerd's unit tests, on the other hand, send a bunch of input very quickly.
Logging all events drowns out the rest of the testing output.

To produce useful output when running in production while producing readable
output from tests, powerd logs messages at the `INFO` level and above by
default, while unit tests log `WARNING` and above.

Please use logging macros as follows within powerd:

| Macro          | Usage |
|----------------|-------|
| `VLOG(1)`      | Debugging info that is hidden by default but can be selectively enabled by developers |
| `LOG(INFO)`    | Input from other systems or actions performed by powerd (i.e. things that would be useful when trying to figure out what powerd was thinking when investigating a bug report) |
| `LOG(WARNING)` | Minor errors (e.g. bad input from other daemons) |
| `LOG(ERROR)`   | Major errors (e.g. problems communicating with the kernel) |
| `LOG(FATAL)`   | Critical problems that make powerd unusable or that indicate problems in the readonly system image (e.g. malformed preference defaults) |
| `CHECK(...)`   | Same as `LOG(FATAL)` |
