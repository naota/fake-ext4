/*
 * fake-ext4.c
 *
 * Copyright (C) 2018 Naohiro Aota <naota@elisp.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/livepatch.h>

#include <linux/magic.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/statfs.h>

#define MY_FSID_VAL0 0x546b23f0
#define MY_FSID_VAL1 0xcd0e1903

int livepatch_user_statfs(const char __user *pathname, struct kstatfs *st)
{
	struct path path;
	int error;
	unsigned int lookup_flags = LOOKUP_FOLLOW|LOOKUP_AUTOMOUNT;
retry:
	error = user_path_at(AT_FDCWD, pathname, lookup_flags, &path);
	if (!error) {
		error = vfs_statfs(&path, st);
		if (!error &&
		    st->f_fsid.val[0] == MY_FSID_VAL0 &&
		    st->f_fsid.val[1] == MY_FSID_VAL1)
			st->f_type = EXT4_SUPER_MAGIC;
		path_put(&path);
		if (retry_estale(error, lookup_flags)) {
			lookup_flags |= LOOKUP_REVAL;
			goto retry;
		}
	}
	return error;
}

static struct klp_func funcs[] = {
	{
		.old_name = "user_statfs",
		.new_func = livepatch_user_statfs,
	},
	{ }
};

static struct klp_object objs[] = {
	{
		/* name being NULL means vmlinux */
		.funcs = funcs,
	}, { }
};

static struct klp_patch patch = {
	.mod = THIS_MODULE,
	.objs = objs,
};

static int livepatch_init(void)
{
	int ret;

	ret = klp_register_patch(&patch);
	if (ret)
		return ret;
	ret = klp_enable_patch(&patch);
	if (ret) {
		WARN_ON(klp_unregister_patch(&patch));
		return ret;
	}
	return 0;
}

static void livepatch_exit(void)
{
	WARN_ON(klp_unregister_patch(&patch));
}

module_init(livepatch_init);
module_exit(livepatch_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");
