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
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <pthread.h>


#define BACKING_DIR "/tmp"
#define FIX_PATH(path) \
	do { \
		if (path[0] == '/') \
			path++; \
		else \
			return -EINVAL; \
	} while (0)

static pthread_mutex_t lock;
static int open_count;
static char opened_filename[FILENAME_MAX + 1];

static int toilet_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		if (lstat(".", stbuf) != 0)
			return -errno;
		return 0;
	}

	FIX_PATH(path);
	if (lstat(path, stbuf) != 0)
		return -errno;
	return 0;
}

static int toilet_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void)offset;
	(void)fi;

	if (strcmp(path, "/") == 0) {
		dp = opendir(".");
	} else {
		FIX_PATH(path);
		dp = opendir(path);
	}

	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int toilet_preopen(const char *path)
{
	int status = 0;

	pthread_mutex_lock(&lock);
	if (open_count > 0) {
		if (strcmp(opened_filename, path) == 0)
			open_count++;
		else
			status = -EACCES;
	} else {
		open_count++;
		if (strlen(path) > FILENAME_MAX)
			status = -EINVAL;
		else
			strcpy(opened_filename, path);
	}
	pthread_mutex_unlock(&lock);

	return status;
}

static int toilet_open(const char *path, struct fuse_file_info *fi)
{
	int status;
	int fd;
	FIX_PATH(path);

	status = toilet_preopen(path);
	if (status == 0) {
		fd = open(path, fi->flags);
		if (fd < 0)
			status = -errno;
		else
			fi->fh = fd;
	}
	return status;
}


static int toilet_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
	int status;
	int fd;
	FIX_PATH(path);

	status = toilet_preopen(path);
	if (status == 0) {
		fd = creat(path, mode);
		if (fd < 0)
			status = -errno;
		else
			fi->fh = fd
;	}
	return status;
}

static int toilet_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	close(fi->fh);
	pthread_mutex_lock(&lock);
	open_count--;
	assert(open_count >= 0);
	assert(strcmp(opened_filename, path) == 0);
	pthread_mutex_unlock(&lock);

	return 0;
}


static int toilet_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi)
{
	return 0;
}

static int toilet_write(const char *path, char *buf, size_t size, off_t offset,
			struct fuse_file_info *fi)
{
	return 0;
}

static int toilet_truncate(const char *path, off_t size)
{
	int status = 0;

	FIX_PATH(path);
	pthread_mutex_lock(&lock);
	if (open_count > 0)
		status = -EACCES;
	pthread_mutex_unlock(&lock);

	if (status == 0) {
		status = truncate(path, size);
		if (status == -1)
			return -errno;
	}

	return status;
}

static struct fuse_operations toilet_oper = {
	.getattr	= toilet_getattr,
	.readdir	= toilet_readdir,
	.open		= toilet_open,
	.create     = toilet_create,
	.release    = toilet_release,
	.read		= toilet_read,
	.read		= toilet_write,
	.truncate	= toilet_truncate,
};

int main(int argc, char *argv[])
{
	int status;

	if (pthread_mutex_init(&lock, NULL) != 0) {
		perror("Failed to init mutex");
		return errno;
	}
	if (chdir(BACKING_DIR) != 0) {
		perror("Failed to change working directory");
		return errno;
	}

	status = fuse_main(argc, argv, &toilet_oper, NULL);
	pthread_mutex_destroy(&lock);
	return status;
}
