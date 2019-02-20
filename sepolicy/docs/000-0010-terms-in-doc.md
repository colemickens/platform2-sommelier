## Terms in This Documentation

This documentation contains many SELinux terms. It will be explained in this
section.

- `SELinux`: short for `Security-Enhanced Linux`. It provides MAC(Mandatory
  Access Control) to Linux. It defines (SELinux) a context for each object no
  matter if it's a process, normal file, directory, link, a socket, etc. When
  the actor(subject) requests to use a permission on the object, it checks the
  predefined policy to see if it's allowed or not. If the policy does not allow
  the action, then the calls that require this permission are denied. Automatic
  transition upon executing, or creation of a file is also possible.
- `security context`: Security context, also known as security label, is a
  string containing multiple parts, used to identify the process to look up
  seucurity rules before granting or denying access to certain permissions.
  - `domain`: the `security context` for a process can be called as `domain`.
  - `parts`: The security context consists of 4 parts,
    `<user>:<role>:<type>:<range>`. For the syslog file `/var/log/messages`, it
    looks like `u:object_r:cros_syslog:s0`, and for a process like upstart, it
    looks like `u:r:cros_init:s0`.
    - `user`: to identify an SELinux user. not related to POSIX user. Chrome OS
      doesn't use multi-user. The only user is `u`.
    - `role`: to identify an SELinux role. Chrome OS doesn't use multi-role. The
      role for files is `object_r`, and for process(including /proc/PID/*) is
      `r`.
    - `type`: this is the most basic and important part in the security context.
      It's a string that independent from each other. For Chrome OS, it's the
      key part to identify the process, or file, for rules look-up.
    - `range`: range contains combinations of security classes and security
      levels. security classes are indepenet from each other, while one security
      levels is dominated by another one. Chrome OS doesn't use multi-class
      security or multi-level security, but ARC container runnning Android
      program is using MCS and MLS.
- `attributes`: attribute is a named group of types.
- `rules`: rules defines whether an access request should be allowed, or logged,
  and how the security context will transit after the access.  Common rules to
  be used in Chrome OS (excluding ARC) SELinux policy are as follows. More
  details will be described in sections about how to write policies.
    - `allow`: `allow contextA contextB:class { permissions }`. Grants
      `contextA` `permissions` access to `class` (e.g. file, sock_file, port)
      under `contextB`. Either contextA or contextB can be changed to a group of
      contexts or `attributes` that can be assigned to multiple contexts.
    - `auditallow`: the same syntax as allow. It will log the access after it's
      being granted. auditallow-only will not grant the access.
    - `dontaudit`: the same syntax as allow. It will stop logging the given
      access after it's being denied.
    - `neverallow`: the same syntax as allow. It is not denying anything at
      runtime. It only performs compile-time check to see whether there's any
      conflict between `allow` and `neverallow`. Android has CTS to test the
      policy running active in the system break their neverallows or not.
    - `allowxperm`: `allowxperm contextA contextB:class permission args`. It
      check the args too at permission request. The arg is ususally not file
      paths but flags, since file paths should already be reflected in the
      context.  `allowxperm`-only will not grant the access. It must be used in
      combination of `allow`. Like `allow`, `allowxperm` also has similiar
      command in `auditallowxperm, `dontauditxperm`, and `neverallowxperm`.
    - `type_transition`: defines how type will auto change to a different one.
      Common type_transitions are:
      - `type_transition olddomain file_context:process newdomain`: when process
        in olddomain executes file_context, the process auto-transits to
        newdomain.
      - `type_transition domain contextA:{dir file ...} contextB [optional
        name]`: when process in domain, create a {dir file ...} optionally named
        as `name`, in `contextA`, the created file are auto-labelled as
        `contextB`. This is only true if the process didn't explicitly set the
        creation context. Without the type_transition, the created dir, file,
        and etc auto-inherits label from its parent.


### Reference

[Security Context](https://selinuxproject.org/page/NB_SC)

[Object Classes and Their
Permissions](https://selinuxproject.org/page/ObjectClassesPerms)
