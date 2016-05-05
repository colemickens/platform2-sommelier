/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE /* For asprintf */

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "container_cgroup.h"
#include "libcontainer.h"
#include "libminijail.h"

struct container_mount {
	char *name;
	char *source;
	char *destination;
	char *type;
	char *data;
	int flags;
	int uid;
	int gid;
	int mode;
	int mount_in_ns;  /* True if mount should happen in new vfs ns */
	int create;  /* True if target should be created if it doesn't exist */
};

struct container_device {
	char type;  /* 'c' or 'b' for char or block */
	char *path;
	int fs_permissions;
	int major;
	int minor;
	int copy_minor; /* Copy the minor from existing node, ignores |minor| */
	int uid;
	int gid;
	int read_allowed;
	int write_allowed;
	int modify_allowed;
};

/*
 * Structure that configures how the container is run.
 *
 * rootfs - Path to the root of the container's filesystem.
 * program_argv - The program to run and args, e.g. "/sbin/init".
 * num_args - Number of args in program_argv.
 * uid_map - Mapping of UIDs in the container, e.g. "0 100000 1024"
 * gid_map - Mapping of GIDs in the container, e.g. "0 100000 1024"
 * alt_syscall_table - Syscall table to use or NULL if none.
 * mounts - Filesystems to mount in the new namespace.
 * num_mounts - Number of above.
 * devices - Device nodes to create.
 * num_devices - Number of above.
 * run_setfiles - Should run setfiles on mounts to enable selinux.
 */
struct container_config {
	char *rootfs;
	char **program_argv;
	size_t num_args;
	char *uid_map;
	char *gid_map;
	char *alt_syscall_table;
	struct container_mount *mounts;
	size_t num_mounts;
	struct container_device *devices;
	size_t num_devices;
	const char *run_setfiles;
};

struct container_config *container_config_create()
{
	return calloc(1, sizeof(struct container_config));
}

void container_config_destroy(struct container_config *c)
{
	size_t i;

	if (c == NULL)
		return;
	free(c->rootfs);
	for (i = 0; i < c->num_args; ++i)
		free(c->program_argv[i]);
	free(c->program_argv);
	free(c->uid_map);
	free(c->gid_map);
	free(c->alt_syscall_table);
	for (i = 0; i < c->num_mounts; ++i) {
		free(c->mounts[i].name);
		free(c->mounts[i].source);
		free(c->mounts[i].destination);
		free(c->mounts[i].type);
		free(c->mounts[i].data);
	}
	free(c->mounts);
	for (i = 0; i < c->num_devices; ++i) {
		free(c->devices[i].path);
	}
	free(c->devices);
	free(c);
}

int container_config_rootfs(struct container_config *c, const char *rootfs)
{
	c->rootfs = strdup(rootfs);
	if (!c->rootfs)
		return -ENOMEM;
	return 0;
}

const char *container_config_get_rootfs(const struct container_config *c)
{
	return c->rootfs;
}

int container_config_program_argv(struct container_config *c,
				  char **argv, size_t num_args)
{
	size_t i;

	c->num_args = num_args;
	c->program_argv = calloc(num_args + 1, sizeof(char *));
	if (!c->program_argv)
		return -ENOMEM;
	for (i = 0; i < num_args; ++i) {
		c->program_argv[i] = strdup(argv[i]);
		if (!c->program_argv[i])
			return -ENOMEM;
	}
	c->program_argv[num_args] = NULL;
	return 0;
}

size_t container_config_get_num_program_args(const struct container_config *c)
{
	return c->num_args;
}

const char *container_config_get_program_arg(const struct container_config *c,
					     size_t index)
{
	if (index >= c->num_args)
		return NULL;
	return c->program_argv[index];
}

int container_config_uid_map(struct container_config *c, const char *uid_map)
{
	c->uid_map = strdup(uid_map);
	if (!c->uid_map)
		return -ENOMEM;
	return 0;
}

int container_config_gid_map(struct container_config *c, const char *gid_map)
{
	c->gid_map = strdup(gid_map);
	if (!c->gid_map)
		return -ENOMEM;
	return 0;
}

int container_config_alt_syscall_table(struct container_config *c,
				       const char *alt_syscall_table)
{
	c->alt_syscall_table = strdup(alt_syscall_table);
	if (!c->alt_syscall_table)
		return -ENOMEM;
	return 0;
}

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
			       int create)
{
	struct container_mount *mount_ptr;

	if (name == NULL || source == NULL ||
	    destination == NULL || type == NULL)
		return -EINVAL;

	mount_ptr = realloc(c->mounts,
			    sizeof(c->mounts[0]) * (c->num_mounts + 1));
	if (!mount_ptr)
		return -ENOMEM;
	c->mounts = mount_ptr;
	c->mounts[c->num_mounts].name = strdup(name);
	if (!c->mounts[c->num_mounts].name)
		return -ENOMEM;
	c->mounts[c->num_mounts].source = strdup(source);
	if (!c->mounts[c->num_mounts].source)
		return -ENOMEM;
	c->mounts[c->num_mounts].destination = strdup(destination);
	if (!c->mounts[c->num_mounts].destination)
		return -ENOMEM;
	c->mounts[c->num_mounts].type = strdup(type);
	if (!c->mounts[c->num_mounts].type)
		return -ENOMEM;
	if (data) {
		c->mounts[c->num_mounts].data = strdup(data);
		if (!c->mounts[c->num_mounts].data)
			return -ENOMEM;
	} else {
		c->mounts[c->num_mounts].data = NULL;
	}
	c->mounts[c->num_mounts].flags = flags;
	c->mounts[c->num_mounts].uid = uid;
	c->mounts[c->num_mounts].gid = gid;
	c->mounts[c->num_mounts].mode = mode;
	c->mounts[c->num_mounts].mount_in_ns = mount_in_ns;
	c->mounts[c->num_mounts].create = create;
	++c->num_mounts;
	return 0;
}

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
				int modify_allowed)
{
	struct container_device *dev_ptr;

	if (path == NULL)
		return -EINVAL;
	/* If using a dynamic minor number, ensure that minor is -1. */
	if (copy_minor && (minor != -1))
		return -EINVAL;

	dev_ptr = realloc(c->devices,
			  sizeof(c->devices[0]) * (c->num_devices + 1));
	if (!dev_ptr)
		return -ENOMEM;
	c->devices = dev_ptr;
	c->devices[c->num_devices].type = type;
	c->devices[c->num_devices].path = strdup(path);
	if (!c->devices[c->num_devices].path)
		return -ENOMEM;
	c->devices[c->num_devices].fs_permissions = fs_permissions;
	c->devices[c->num_devices].major = major;
	c->devices[c->num_devices].minor = minor;
	c->devices[c->num_devices].copy_minor = copy_minor;
	c->devices[c->num_devices].uid = uid;
	c->devices[c->num_devices].gid = gid;
	c->devices[c->num_devices].read_allowed = read_allowed;
	c->devices[c->num_devices].write_allowed = write_allowed;
	c->devices[c->num_devices].modify_allowed = modify_allowed;
	++c->num_devices;
	return 0;
}

void container_config_run_setfiles(struct container_config *c,
				   const char *setfiles_cmd)
{
	c->run_setfiles = setfiles_cmd;
}

const char *container_config_get_run_setfiles(const struct container_config *c)
{
	return c->run_setfiles;
}

/*
 * Container manipulation
 */
struct container {
	struct container_cgroup *cgroup;
	struct minijail *jail;
	pid_t init_pid;
	char *runfs;
	char *rundir;
	char *runfsroot;
	char *pid_file_path;
	char **ext_mounts; /* Mounts made outside of the minijail */
	size_t num_ext_mounts;
	const char *name;
};

struct container *container_new(const char *name,
				const char *rundir)
{
	struct container *c;

	c = calloc(1, sizeof(*c));
	if (!c)
		return NULL;
	c->name = name;
	c->cgroup = container_cgroup_new(name, "/sys/fs/cgroup");
	c->rundir = strdup(rundir);
	if (!c->cgroup || !c->rundir) {
		container_destroy(c);
		return NULL;
	}
	return c;
}

void container_destroy(struct container *c)
{
	if (c->cgroup)
		container_cgroup_destroy(c->cgroup);
	free(c->rundir);
	free(c);
}

static int make_dir(const char *path, int uid, int gid, int mode)
{
	if (mkdir(path, mode))
		return -errno;
	if (chmod(path, mode))
		return -errno;
	if (chown(path, uid, gid))
		return -errno;
	return 0;
}

static int touch_file(const char *path, int uid, int gid, int mode)
{
	int rc;
	int fd = open(path, O_RDWR | O_CREAT, mode);
	if (fd < 0)
		return -errno;
	rc = fchown(fd, uid, gid);
	close(fd);

	if (rc)
		return -errno;
	return 0;
}

/* Make sure the mount target exists in the new rootfs. Create if needed and
 * possible.
 */
static int setup_mount_destination(const struct container_mount *mnt,
				   const char *source,
				   const char *dest)
{
	int rc;
	struct stat st_buf;

	rc = stat(dest, &st_buf);
	if (rc == 0) /* destination exists */
		return 0;

	/* Try to create the destination. Either make directory or touch a file
	 * depending on the source type.
	 */
	rc = stat(source, &st_buf);
	if (rc || S_ISDIR(st_buf.st_mode) || S_ISBLK(st_buf.st_mode))
		return make_dir(dest, mnt->uid, mnt->gid, mnt->mode);

	return touch_file(dest, mnt->uid, mnt->gid, mnt->mode);
}

/* Fork and exec the setfiles command to configure the selinux policy. */
static int run_setfiles_command(const struct container *c,
				const struct container_config *config,
				const char *dest)
{
	int rc;
	int status;
	int pid;
	char *context_path;

	if (!config->run_setfiles)
		return 0;

	if (asprintf(&context_path, "%s/file_contexts",
		     c->runfsroot) < 0)
		return -errno;

	pid = fork();
	if (pid == 0) {
		const char *argv[] = {
			config->run_setfiles,
			"-r",
			c->runfsroot,
			context_path,
			dest,
			NULL,
		};
		const char *env[] = {
			NULL,
		};

		execve(argv[0], (char *const*)argv, (char *const*)env);

		/* Command failed to exec if execve returns. */
		_exit(-errno);
	}
	free(context_path);
	if (pid < 0)
		return -errno;
	do {
		rc = waitpid(pid, &status, 0);
	} while (rc == -1 && errno == EINTR);
	if (rc < 0)
		return -errno;
	return status;
}

/*
 * Unmounts anything we mounted in this mount namespace in the opposite order
 * that they were mounted.
 */
static int unmount_external_mounts(struct container *c)
{
	int ret = 0;

	while (c->num_ext_mounts) {
		c->num_ext_mounts--;
		if (umount(c->ext_mounts[c->num_ext_mounts]))
			ret = -errno;
		free(c->ext_mounts[c->num_ext_mounts]);
	}
	free(c->ext_mounts);
	return ret;
}

static int do_container_mounts(struct container *c,
			       const struct container_config *config)
{
	unsigned int i;
	char *source;
	char *dest;

	/*
	 * Allocate space to track anything we mount in our mount namespace.
	 * This over-allocates as it has space for all mounts.
	 */
	c->ext_mounts = calloc(config->num_mounts, sizeof(*c->ext_mounts));
	if (!c->ext_mounts)
		return -errno;

	for (i = 0; i < config->num_mounts; ++i) {
		const struct container_mount *mnt = &config->mounts[i];

		source = NULL;
		dest = NULL;
		if (asprintf(&dest, "%s%s", c->runfsroot, mnt->destination) < 0)
			return -errno;

		/*
		 * If it's a bind mount relative to rootfs, append source to
		 * rootfs path, otherwise source path is absolute.
		 */
		if ((mnt->flags & MS_BIND) && mnt->source[0] != '/') {
			if (asprintf(&source, "%s/%s", c->runfsroot,
				     mnt->source) < 0)
				goto error_free_return;
		} else {
			if (asprintf(&source, "%s", mnt->source) < 0)
				goto error_free_return;
		}

		if (mnt->create) {
			if (setup_mount_destination(mnt, source, dest))
				goto error_free_return;
		}
		if (mnt->mount_in_ns) {
			/* We can mount this with minijail. */
			if (minijail_mount(c->jail, source, mnt->destination,
					   mnt->type, mnt->flags))
				goto error_free_return;
		} else {
			/*
			 * Mount this externally and unmount it on exit. Don't
			 * allow execution from external mounts.
			 */
			if (mount(source, dest, mnt->type,
				  mnt->flags | MS_NOEXEC, mnt->data))
				goto error_free_return;
			/* Save this to unmount when shutting down. */
			c->ext_mounts[c->num_ext_mounts] = strdup(dest);
			c->num_ext_mounts++;
		}
		free(source);
		free(dest);
	}
	return 0;

error_free_return:
	free(dest);
	free(source);
	unmount_external_mounts(c);
	return -errno;
}

int container_start(struct container *c, const struct container_config *config)
{
	int rc;
	unsigned int i;
	const char *rootfs = config->rootfs;
	char *runfs_template;

	if (!config)
		return -EINVAL;
	if (!config->program_argv || !config->program_argv[0])
		return -EINVAL;

	if (asprintf(&runfs_template, "%s/%s_XXXXXX", c->rundir, c->name) < 0)
		return -errno;

	c->runfs = mkdtemp(runfs_template);
	if (!c->runfs) {
		free(runfs_template);
		return -errno;
	}
	if (asprintf(&c->runfsroot, "%s/root", c->runfs) < 0) {
		free(runfs_template);
		return -errno;
	}

	rc = mkdir(c->runfsroot, 0660);
	if (rc)
		goto error_rmdir;

	rc = mount(rootfs, c->runfsroot, "", MS_BIND | MS_RDONLY | MS_NOEXEC,
		   NULL);
	if (rc)
		goto error_rmdir;

	c->jail = minijail_new();

	if (do_container_mounts(c, config))
		goto error_rmdir;

	c->cgroup->ops->deny_all_devices(c->cgroup);

	for (i = 0; i < config->num_devices; i++) {
		const struct container_device *dev = &config->devices[i];
		int mode;
		int minor = dev->minor;

		switch (dev->type) {
		case  'b':
			mode = S_IFBLK;
			break;
		case 'c':
			mode = S_IFCHR;
			break;
		default:
			goto error_rmdir;
		}
		mode |= dev->fs_permissions;

		if (dev->copy_minor) {
			struct stat st_buff;
			if (stat(dev->path, &st_buff) < 0)
				goto error_rmdir;
			/* Use the minor macro to extract the device number. */
			minor = minor(st_buff.st_rdev);
		}
		if (minor >= 0) {
			char *path;

			if (asprintf(&path, "%s%s", c->runfsroot, dev->path) < 0)
				goto error_rmdir;
			rc = mknod(path, mode, makedev(dev->major, minor));
			if (rc && errno != EEXIST) {
				free(path);
				goto error_rmdir;
			}
			rc = chown(path, dev->uid, dev->gid);
			if (rc) {
				free(path);
				goto error_rmdir;
			}
			rc = chmod(path, dev->fs_permissions);
			free(path);
			if (rc)
				goto error_rmdir;
		}

		rc = c->cgroup->ops->add_device(c->cgroup, dev->major,
						minor, dev->read_allowed,
						dev->write_allowed,
						dev->modify_allowed, dev->type);
		if (rc)
			goto error_rmdir;
	}

	/* Potentailly run setfiles on mounts configured outside of the jail */
	for (i = 0; i < config->num_mounts; i++) {
		const struct container_mount *mnt = &config->mounts[i];
		char *dest;

		if (mnt->mount_in_ns)
			continue;
		if (asprintf(&dest, "%s%s", c->runfsroot, mnt->destination) < 0)
			goto error_rmdir;
		rc = run_setfiles_command(c, config, dest);
		free(dest);
		if (rc)
			goto error_rmdir;
	}

	/* Setup and start the container with libminijail. */
	if (asprintf(&c->pid_file_path, "%s/container.pid", c->runfs) < 0)
		goto error_rmdir;
	minijail_write_pid_file(c->jail, c->pid_file_path);
	minijail_reset_signal_mask(c->jail);

	/* Setup container namespaces. */
	minijail_namespace_ipc(c->jail);
	minijail_namespace_vfs(c->jail);
	minijail_namespace_net(c->jail);
	minijail_namespace_pids(c->jail);
/*	TODO(dgreid) - Enable user namespaces
	minijail_namespace_user(c->jail);
	rc = minijail_uidmap(c->jail, config->uid_map);
	if (rc)
		goto error_rmdir;
	rc = minijail_gidmap(c->jail, config->gid_map);
	if (rc)
		goto error_rmdir;
*/

	rc = minijail_enter_pivot_root(c->jail, c->runfsroot);
	if (rc)
		goto error_rmdir;

	/* Add the cgroups configured above. */
	rc = minijail_add_to_cgroup(c->jail, cgroup_cpu_tasks_path(c->cgroup));
	if (rc)
		goto error_rmdir;
	rc = minijail_add_to_cgroup(c->jail,
				    cgroup_cpuacct_tasks_path(c->cgroup));
	if (rc)
		goto error_rmdir;
	rc = minijail_add_to_cgroup(c->jail,
				    cgroup_devices_tasks_path(c->cgroup));
	if (rc)
		goto error_rmdir;
	rc = minijail_add_to_cgroup(c->jail,
				    cgroup_freezer_tasks_path(c->cgroup));
	if (rc)
		goto error_rmdir;

	if (config->alt_syscall_table)
		minijail_use_alt_syscall(c->jail, config->alt_syscall_table);

	minijail_run_as_init(c->jail);

	/* TODO(dgreid) - remove this once shared mounts are cleaned up. */
	minijail_skip_remount_private(c->jail);

	/* Last mount is to make '/' executable in the container. */
	rc = minijail_mount(c->jail, rootfs, "/", "",
			    MS_REMOUNT | MS_RDONLY);
	if (rc)
		goto error_rmdir;

	rc = minijail_run_pid_pipes_no_preload(c->jail,
					       config->program_argv[0],
					       config->program_argv,
					       &c->init_pid, NULL, NULL,
					       NULL);
	if (rc)
		goto error_rmdir;
	return 0;

error_rmdir:
	umount(c->runfsroot);
	rmdir(c->runfsroot);
	if (c->pid_file_path)
		unlink(c->pid_file_path);
	free(c->pid_file_path);
	rmdir(c->runfs);
	free(c->runfsroot);
	free(c->runfs);
	return rc;
}

const char *container_root(struct container *c)
{
	return c->runfs;
}

int container_pid(struct container *c)
{
	return c->init_pid;
}

static int container_teardown(struct container *c)
{
	int ret = 0;

	unmount_external_mounts(c);
	if (umount(c->runfsroot))
		ret = -errno;
	if (rmdir(c->runfsroot))
		ret = -errno;
	if (c->pid_file_path && unlink(c->pid_file_path))
		ret = -errno;
	if (rmdir(c->runfs))
		ret = -errno;
	free(c->pid_file_path);
	free(c->runfsroot);
	free(c->runfs);
	return ret;
}

int container_wait(struct container *c)
{
	int rc;

	do {
		rc = minijail_wait(c->jail);
	} while (rc == -1 && errno == EINTR);

	if (rc >= 0)
		rc = container_teardown(c);
	return rc;
}

int container_kill(struct container *c)
{
	int rc;

	rc = kill(c->init_pid, SIGKILL);
	if (rc)
		return -errno;
	return container_wait(c);
}
