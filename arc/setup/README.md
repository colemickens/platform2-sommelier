# Chrome OS arc-setup.

`arc-setup` handles setup/teardown of ARC container or upgrading container.
For example, mount point creation, directory creation, setting permissions uids
and gids, selinux label setting, config file creation.

Often, script language is used for such stuff in general, but ARC uses native
executable just for performance and better testability.
