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

1. A quick command could test whether your program works by putting the whole
system permissive. By executing `setenforce 0` as root in developer mode, you
can put the whole system permissive. You'll be able to test if your program
comes to work.

1. If you program is a daemon process which fails so early before you can have a
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

#### Analyzing audit logs

The most important and fundamental way to debug your policy is to read the audit
log.

In the previous audit log example, we know it's "`cat`" process in
`cros_ssh_session` domain was denied to read file "`messages`" in device `dm-0`
labelled as `cros_syslog`.

There're the main things to look at in the audit logs.

- Actor
  - Which process, pid / name;
  - The context of the actor (scontext);

- Actee / Target
  - Which target?
  - Which class. process, file, sock_file, port, etc?

By looking at the log, the main thinkabout would be"

- Is the actor and target at the correct context? No? => Fix the context.
  - File label:
    - In system image: add the context to
      `platform2/sepolicy/file_contexts/chromeos_file_contexts`, and
      `platform2/sepolicy/sepolicy/chromeos/file.te`.
    - In stateful partition:
      - (Recommended and Easiest) Introduce a new file type in
        `.../sepolicy/chromeos/file.te`, and use `filetrans_pattern` macro to
        allow auto assigning labels upon file creation.
      - Introduce a new file type in `.../sepolicy/chromeos/file.te`, and modify
        the program to set correct creation label.
        [setfscreatecon(3)](https://manpages.ubuntu.com/manpages/bionic/en/man3/setfscreatecon.3.html)
  - Process domain: fix the executable file label, and use `domain_auto_trans`
    macros if possible.
  - Port context, etc: you're probably already an advanced SELinux policy writer
    if you met this point. You can refer to [SELinux Project
    Wiki](https://selinuxproject.org/), for example
    [portcon](https://selinuxproject.org/page/NetworkStatements#portcon)
- Is the action really needed? No? => Fix the program to eliminate the action.

  Examples of mostly unneeded actions:
  - relabelfrom/relabelto: only some startup script should need this.
  - capability dac_override: in most cases, it could be avoided by reordering
    chown / actual read-write.
  - mount/mounton: except for some startup script, or programs using libminijail.
    This should be avoided. For programs using minijail0 wrapper, `-T static`
    mode is strongly recommended to leave all the high privileged permissions to
    minijail.

#### Inspecting the runtime state

- File labels: `ls -Z file` or `ls -Zd directory`
- Process domains: `ps -Z`, `ps auxZ`, `ps -Zp <PID>`
- Current domain: `id -Z`
- Run in a different domain: `runcon <context> <program> <args...>` for example,
  `runcon u:r:cros_init:s0 id -Z`. The transition from current domain to new
  domain must be allowed to let this work. Currently, either `cros_ssh_session`
  or `frecon` is running permissive, it should always work if you're executing
  in the console, or via ssh.

#### Update the policy

After understanding why the denials occurred by reading the log, policy may need
updating to fix the problem.

##### Locate the policy

In general, Chrome OS policy lives in `sepolicy` directory in
`chromiumos/platform2` project, which is `src/platform2/sepolicy` in the repo
tree checkout.

A quick grep on the scontext will locate the where it is defined, and most of
its policies.

For example, if we want to change `cros_ssh_session`:

```
$ grep cros_ssh_session . -R
./policy/chromeos/dev/cros_ssh_session.te:type cros_ssh_session, domain, chromeos_domain;
./policy/chromeos/dev/cros_ssh_session.te:permissive cros_ssh_session;
./policy/chromeos/dev/cros_ssh_session.te:typeattribute cros_ssh_session netdomain;
./policy/chromeos/dev/cros_sshd.te:domain_auto_trans(cros_sshd, sh_exec, cros_ssh_session);
./policy/chromeos/file.te:filetrans_pattern(cros_ssh_session, cros_etc, cros_ld_conf_cache, file, "ld.so.cache~");
./policy/chromeos/log-and-errors/cros_crash.te:-cros_ssh_session
```

We can see it's defined in `cros_ssh_session.te`, which means most of our
changes should live in that file.

##### Searching the compiled policy file

`sesearch` is an excellent tool to search inside a compiled policy. You can use
this tool to search what is allowed, what denials are not logged, what grants
are logged, and type transitions, etc.

on Debian-based systems (or gLinux), `sudo apt install setools` will install
this tool.

You can search a policy file, say, `policy.30`, in following examples:

```
# Search all allow rule with scontext to be cros_ssh_session or attributes
cros_ssh_session attributes to, tcontext to be cros_sshd or attributes cros_sshd
attributes to with class to be process
$ sesearch policy.30 -A -s cros_ssh_session -t cros_sshd -c process
# Search all type transitions with scontext to be exactly minijail
$ sesearch policy.30 -T -s minijail -ds
```

`man sesearch` will provide all the options to search `allow`, `auditallow`,
`dontaudit`, `allowxperm`, etc, with filters on scontext, tcontext, class,
permissions.

##### Put domain to permissive

Sometimes, during debugging, you may not to want to put the system permissive.
You can put only one domain permissive.

1. Locate the policy file as above.

1. Simply add `permissive <domain type>`, for example, `permissive
cros_ssh_session` will put `cros_ssh_session` to permissive.

This will only put the given domain to permissive, and everything with the
permissive actor domain (scontext) will not actually being denied.

But please note, some operation may indicate other permission at runtime. For
example, file creation will check
`{ associate } scontext=file_type tcontext=fs_type class=filesystem `, these kind of
denials may occur. If you saw similiar denials please reach kroot@ or fqj@,
we'll fix it.

##### Writing policy fix

1. Identify whether labebling files is needed. If yes, label the files either in
   file_contexts or via type_transition.

1. Fix the program or add `dontaudit` rule to prevent from spamming logs if it
   shouldn't be allowed.

1. Write allow rule or allowxperm rule based on denials seen, and the behavior
   analysis of the program.

   1. for one-time program-specific permission requests, simply `allow[xperm]
      scontext tcontext:class perms [args for allowxperm];`
      `scontext`, `tcontext`, and `perms` can be plural in format like `{ a b c
      }`

   1. for permission requests that may apply to other programs, create an
      attribute and attribute current domain to it. And write corresponding
      rules for that attribute.

   1. use m4 macros wisely, we have many useful macros like `r_file_perms`,
      `rw_file_perms`, `create_file_perms`, `filetrans_pattern`, and
      `domain_auto_trans`.

For more details in writing policy, please refer to previous sections about
writing policies.

#### Useful build flags for debug

1. `USE="selinux_develop"`: log permissive denials and make sure log is almost
   not suppressed by printk limit.

1. `USE="selinux_experimental"`: build with SELinux mode in permissive by
   default. This is equivlant to manually changing `SELINUX=permissive` in
   `/etc/selinux/config`

1. `USE="selinux_audit_all"`: remove all the `dontaudit` rule before compiling
   to the final monolithic policy. There're some should-be-denied access with
   `dontaudit` rules, so denials don't spam the log. But you may want to see
   them sometimes during development or debugging process.

For Googlers, there's a nice introduction presentation slides how debugging
SELinux policies to refer to though it's for Android, at
[go/sepolicy-debug](https://goto.google.com/sepolicy-debug)
