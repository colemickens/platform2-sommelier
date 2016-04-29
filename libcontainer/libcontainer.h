/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CONTAINER_MANAGER_LIBCONTAINER_H_
#define CONTAINER_MANAGER_LIBCONTAINER_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct container_config;

/* Create a container config. */
struct container_config *container_config_create();

/* Destroy a config create with container_config_create. */
void container_config_destroy(struct container_config *c);

/* rootfs - Path to the root of the container's filesystem. */
int container_config_rootfs(struct container_config *c, const char *rootfs);

/* The program to run and args, e.g. "/sbin/init", "--second-stage". */
int container_config_program_argv(struct container_config *c,
				  char **argv, size_t num_args);

/* The pid of the program will be written here. */
int container_config_pid_file(struct container_config *c, const char *path);

/* Mapping of UIDs in the container, e.g. "0 100000 1024" */
int container_config_uid_map(struct container_config *c, const char *uid_map);

/* Mapping of GIDs in the container, e.g. "0 100000 1024" */
int container_config_gid_map(struct container_config *c, const char *gid_map);

/* Alt-Syscall table to use or NULL if none. */
int container_config_alt_syscall_table(struct container_config *c,
				       const char *alt_syscall_table);

/*
 * Add a filesystem to mount in the new VFS namespace.
 *
 * c - The container config in which to add the mount.
 * source - Mount source, e.g. "tmpfs" or "/data".
 * destination - Mount point in the container, e.g. "/dev".
 * type - Mount type, e.g. "tmpfs", "selinuxfs", or "devpts".
 * data - Mount data for extra options, e.g. "newinstance" or "ptmxmode=0000".
 * flags - Mount flags as defined in mount(2);
 * uid - uid to chown mount point to if created.
 * gid - gid to chown mount point to if created.
 * mode - Permissions of mount point if created.
 * mount_in_ns - True if mount should happen in the process' vfs namespace.
 * create - If true, create mount destination if it doesn't exist.
 */
int container_config_add_mount(struct container_config *c,
			       const char *name,
			       const char *source,
			       const char *destination,
			       const char *type,
			       const char *data,
			       int flags,
			       int uid,
			       int gid,
			       int mode,
			       int mount_in_ns,
			       int create);

/*
 * Add a device node to create.
 *
 * c - The container config in which to add the mount.
 * type - 'c' or 'b' for char or block respectively.
 * path - Where to mknod, "/dev/zero".
 * fs_permissions - Permissions to set on the node.
 * major - Major device number.
 * minor - Minor device number.
 * copy_minor - Overwrite minor with the minor of the existing device node.  If
 *   this is true minor will be copied from an existing node.  The |minor| param
 *   should be set to -1 in this case.
 * uid - User to own the device.
 * gid - Group to own the device.
 * read_allowed - If true allow reading from the device via "devices" cgroup.
 * write_allowed - If true allow writing to the device via "devices" cgroup.
 * modify_allowed - If true allow creation of the device via "devices" cgroup.
 */
int container_config_add_device(struct container_config *c,
				char type,
				const char *path,
				int fs_permissions,
				int major,
				int minor,
				int copy_minor,
				int uid,
				int gid,
				int read_allowed,
				int write_allowed,
				int modify_allowed);

/*
 * Set to cause the given setfiles command to be run whenever a mount is made
 * in the parent mount namespace.
 */
void container_config_run_setfiles(struct container_config *c,
				   const char *setfiles_cmd);

/* Container manipulation. */
struct container;

/*
 * Create a container based on the given config.
 *
 * name - Name of the directory holding the container config files.
 * rundir - Where to build the temporary rootfs.
 * config - Details of how the container should be run.  The caller of the
 *   function retains ownership of the config struct.
 */
struct container *container_new(const char *name,
				const char *rundir,
				const struct container_config *config);

/* Destroy a container created with container_new. */
void container_destroy(struct container *c);

/* Start the container. Returns 0 on success. */
int container_start(struct container *c);

/* Get the path to the root of the container. */
const char *container_root(struct container *c);

/* Get the pid of the init process in the container. */
int container_pid(struct container *c);

/* Wait for the container to exit. Returns 0 on success. */
int container_wait(struct container *c);

/* Kill the container's init process, then wait for it to exit. */
int container_kill(struct container *c);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif  /* CONTAINER_MANAGER_LIBCONTAINER_H_ */
