
## SELinux in Chrome OS boot process

SELinux has no presence until init (upstart) loads the policy, for example, in
the bootloader. This section will not discuss the stage before initially loading
the policy.

1. init loads the selinux policy based on configs in /etc/selinuc/config, and
   mounts the selinuxfs to /sys/fs/selinux. init will be assigned with initial
   contexts (kernel).

2. init re-execs itself, to trigger type_transition `type_transition kernel
   chromeos_init_exec:process cros_init`, which will auto-transits init into
   cros_init domain.

3. init starting up service. init will startup services, and kernel will
   auto-transits based on defined type transition rules. See next section for
   details.

### init starting up services

init will start services based on the config files in /etc/init/ and their
dependencies. The "service" here not only include the daemon process service,
but also some pre-startup, or short-lived script.

#### Simple startup script embedded in init config file

Simple service startups are simply written in `<service-name>.conf` like

```
exec /sbin/minijail0 -l --uts -i -v -e -t -P /var/empty -T static \
    -b / -b /dev,,1 -b /proc \
    -k tmpfs,/run,tmpfs,0xe -b /run/systemd/journal,,1 \
    -k tmpfs,/var,tmpfs,0xe -b /var/log,,1 -b /var/lib/timezone \
    /usr/sbin/rsyslogd -n -f /etc/rsyslog.chromeos -i /tmp/rsyslogd.pid
```
in syslog.conf, or

```
script
  ARGS=""
  case ${WPA_DEBUG} in
    excessive) ARGS='-ddd';;
    msgdump)   ARGS='-dd';;
    debug)     ARGS='-d';;
    info)      ARGS='';;
    warning)   ARGS='-q';;
    error)     ARGS='-qq';;
  esac
  exec minijail0 -u wpa -g wpa -c 3000 -n -i -- \
    /usr/sbin/wpa_supplicant -u -s ${ARGS} -O/run/wpa_supplicant
end script
```
in wpa_supplicant.conf.

The earlier one, without a small script, kernel SELinux subsystem will
auto-transits the domain from `u:r:cros_init:s0`, `u:r:minijail:s0` upon
executing `/sbin/minijail0`, and then auto-transitst to `u:r:cros_rsyslogd:s0`
upon executing `/usr/sbin/rsyslogd` so the AVC for domain `u:r:cros_rsyslogd:s0`
can be applied to the process. From now in the process, anything file access,
port usages, network usages, capability request, module load, and etc, will be
checked against AVC rules with scontext (source context) being
`u:r:cros_rsyslogd:s0`.

While the latter one, with a small script between `script` and `end script`, of
course, init forks a subprocess (still under cros_init domain) to execute shell,
upon executing shell, this will transits the subprocess to
`u:r:cros_init_scripts:s0` domain, some simple AVCs could be added to cover all
the simple embedded scripts together. Complex script or script needing
permissions more than file, or directory read, write, or creation, or exec,
should be avoided in the simple script, and should use a separate script. Within
the script, it will auto-transits to other domains upon executing the service
program, directly(for example, `exec /usr/sbin/rsyslogd` or indirectly (via
minijail0 the same as above).

#### Separate script to start the service

But there're some more complex service startup scripts, which are written in a
separate (shell) script. The init config file will look like `exec
/path/to/script.sh` or `exec /bin/sh /path/to/script.sh`

The earlier one is always preferred since it tells the kernel exact script being
executed, so automatic domain transition can be feasible upon executing the
script, to not to mix up permission requirements of the single script to the
whole init scripts.

The latter one should be **avoided** _if the script has complex permission
requirements_, like special capabilities, create device, modify sysfs, mount
filesystem, or load kernel modules. Using the latter approach will make the
kernel not able to distinguish which script to be executed, since the exec
syscall are always the same to exec `/bin/sh`, even the same with `script ...
end script` simple embedded script.

If using the earlier one in the init config file, it will auto transits to a
custom defined domain, let's say `u:r:cros_service_a_startup:s0`,  AVC rules
will be defined for this special domain, and finally a type_transition rule will
transit it from `u:r:cros_service_a_startup:s0`  to the domain owning the
service itself, like `u:r:cros_service_a:s0`. In the new domain, it will confine
the rules for the service.

#### Pre-start, pre-shutdown, and post-stop

pre-start, pre-shutdown, or post-stop scripts are usually simple embedded
scripts like

```
pre-start script
  mkdir -p -m 0750
  /run/wpa_supplicant
  chown wpa:wpa /run/wpa_supplicant
end script
```
in wpa_supplicant.conf.

This can be confined together with `u:r:cros_init_script:s0` since

 - It's simple enough, and doing almost similar things as all other simple
   pre-start, pre-shutdown, or post-stop domains;
 - Chrome OS has system image verification to make sure everything under
   /etc/init is the same as original state.

Like startup script, there're still very small number of services, using an
external script file for the startup. For example `pre-start exec
/usr/share/cros/init/shill-pre-start.sh` in shill.conf. This can be either
separate domains if they involves complex permissions like mounting/umounting
filesystems, loading/unloading kernel modules, or special capabilities.
