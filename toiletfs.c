/*
  toiletfs: the one-at-a-time filesystem.
  Copyright (C) 2012  Scott J. Goldman <scottjg@github.com>

  This program can be distributed under the terms of the MIT License.
  See the file COPYING.
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>


static int toilet_getattr(const char *path, struct stat *stbuf)
{
	return 0;
}

static int toilet_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi)
{
	return 0;
}

static int toilet_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int toilet_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi)
{
	return 0;
}

static struct fuse_operations toilet_oper = {
	.getattr	= toilet_getattr,
	.readdir	= toilet_readdir,
	.open		= toilet_open,
	.read		= toilet_read,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &toilet_oper, NULL);
}
