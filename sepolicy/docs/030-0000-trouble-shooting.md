## Troubleshooting

### How to rule out SELinux a possible cause of a problem

Let's assume you write your own program, or modified a program. Suppose it
doesn't work along with the system, you're sure you didn't implement anything
wrong, and suspect it could be SELinux denying some operations.

There're three approaches to identify an potential SELinux problem.

1. Currently, Chrome OS doesn't register any audit daemon, the `[kauditd]` in
   kernel will handle audits, and should output to syslog, as well as serial
   port.  You could dig into the audit log by grepping `avc:` or `audit:`. If
   there's a denial in non-permissive domain, there should be audit messages
   being logged, with `permissive=0`.  You could read the audit log to see if
   it's related to your program.

  - If you're on `betty` or other cros_vm instance, serial port output can be
    found at `/tmp/cros_vm_*/kvm.monitor.serial` in the cros_sdk chroot
    environment.
  - syslog will be logged to `/var/log/messages` and systemd-journald. You can
    read the log by reading `/var/log/messages` or by executing `journalctl`.
    Please note the messages file could be rotated to
    `/var/log/messages.{1,2,3,4,...}` if the system is running long term.

  You should be able to find `permissive=0` in above log locations. If you saw
  some denials with `permissive=1`, it doesn't mean it's denied.  `permissive=1`
  only mean this access it not allowed by policy, but SELinux is still allowing
  it because the domain is not enforced.

2. A quick command could test whether your program works by putting the whole
system permissive. By executing `setenforce 0` as root in developer mode, you
can put the whole system permissive. You'll be able to test if your program
comes to work.

3. If you program is a daemon process which fails so early before you can have a
console access, you could change the SELinux config file located at
`/etc/selinux/config` to
```
SELINUX=permissive
SELINUXTYPE=arc
```
Please keep
`SELINUXTYPE=arc` unchanged, and only changing `SELINUX=` line to `permissive`.
Please don't change it to `disabled` otherwise your system may fail to boot
since init will halt when it fails to load an SELinux policy.

Approach 2 and 3 to put the whole system permissive won't give you any useful
information on what's wrong. Audit log won't print even the policy says to deny
this because the whole system is permissive. It only gives you a true or false
answer, you'll need the audit logs to find out exact what the problem is. We'll
talk more at the how to debug section.

### How to read the denials in audit logs

An AVC audit message looks like

```
audit: type=1400 audit(1550558262.594:5434): avc:  denied  { read } for
pid=26768 comm="cat" name="messages" dev="dm-0" ino=40
scontext=u:r:cros_ssh_session:s0 tcontext=u:object_r:cros_syslog:s0 tclass=file
permissive=1
```

We'll walk through this audit log as an example.

- `type=1400` means it's an AVC audit log. So it's not what we care about. In
  most cases, you're looking at this kind of audit logs.
- `denied` means this permission usage is denied. The other result here could be
  `granted`, `granted` is only printed the the AVC rule has **auditallow** rules
  like `auditallow domainA domainB:file read`.
- `{ read }` means the permission requested is `read`, there could be many
  different kind of permissions, like `open`, `execute`, `append`, `name_bind`,
  `unlink`, etc. The permission inside `{}` could be more than one in one audit
  message, like `{ read open }`.
- `pid=26768` means the pid of the process.
- `comm="cat"` means the program command is cat. It's identical (but truncated)
  to /proc/PID/comm
- `name="messages"`, `dev="dm-0"`, `ino=40` are related information. Please read
  the link below in SELinux Project Wiki to know more.
- `scontext=u:r:cros_ssh_session:s0` means the source context for this
  permission request. If it's a process, it means the domain for the acting
  process.
- `tcontext=u:object_r:cros_syslog:s0` means the target context for this
  permission request. The tcontext could be file labels if it's a file (open,
  read, write, create, etc), process domain (use fd, netlink socket read/write,
  /proc/PID/ to read process attributes, unix socket sendto, etc), filesystem
  type (associate cros_var_lib on labeledfs, associate logger_device on
  devtmpfs, etc), and etc.
- `tclass=file` means the class type of the acting target. For example, in this
  case, it's `file`, it could be `udp_socket`, `fd`, `capability`,
  `netlink_kobject_uevent_socket`, etc.
- `permissive=1` means current audit is permissive or not. If it's permissive,
  it's not really getting denied, otherwise it really denies this permission
  request.

For more details, it can be referred from

[SELinux Project Wiki: NB AL](https://selinuxproject.org/page/NB_AL)

[Red Hat Documentation: Raw Audit
Messages](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/6/html/security-enhanced_linux/sect-security-enhanced_linux-fixing_problems-raw_audit_messages)

[Gentoo Wiki: Where to find SELinux permission denial
details](https://wiki.gentoo.org/wiki/SELinux/Tutorials/Where_to_find_SELinux_permission_denial_details)

[CentOS Wiki: Troubleshooting
SELinux](https://wiki.centos.org/HowTos/SELinux#head-02c04b0b030dd3c3d58bb7acbbcff033505dd3af)

### How to debug your SELinux policy

TODO
