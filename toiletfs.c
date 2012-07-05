/*
  toiletfs: the one-at-a-time filesystem.
  Copyright (C) 2012  Scott J. Goldman <scottjg@github.com>

  This program can be distributed under the terms of the MIT License.
  See the file COPYING.
*/

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE 500
#define _BSD_SOURCE 1

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <assert.h>
#include <pthread.h>

static pthread_mutex_t lock;
static int open_count;
static char opened_filename[FILENAME_MAX + 1];

static struct {
	char *backing_dir;
	char *close_hook;
	int max_files;
} toilet_conf;

#define FIX_PATH(path) \
	do { \
		if (path[0] == '/') \
			path++; \
		else \
			return -EINVAL; \
	} while (0)



static int toilet_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));

	FIX_PATH(path);
	if (strcmp(path, "") == 0) {
		if (lstat(".", stbuf) != 0)
			return -errno;
		return 0;
	}

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

	FIX_PATH(path);
	if (strcmp(path, "") == 0) {
		dp = opendir(".");
	} else {
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

static void exec_hook(const char *path)
{
	pid_t pid;
	if (!toilet_conf.close_hook)
		return;

	pid = fork();
	if (pid == 0) {
		execl(toilet_conf.close_hook, toilet_conf.close_hook,
		      path, NULL);
		_exit(0);
	}
	wait(NULL);
}

static void toilet_preclose(const char *path, int skip_hook)
{
	int last_ref = 0;

	pthread_mutex_lock(&lock);
	open_count--;
	assert(open_count >= 0);
	assert(strcmp(opened_filename, path) == 0);
	if (open_count == 0)
		last_ref = 1;
	pthread_mutex_unlock(&lock);

	if (!skip_hook && last_ref)
		exec_hook(path);

}

static int toilet_flush_cores(const char *path)
{
	DIR *dp;
	int status = 0;
	int num_cores = 0;
	int max_filename = pathconf(path, _PC_NAME_MAX) + 1;
	int len = offsetof(struct dirent, d_name) + max_filename;
	struct dirent *de, *entry = malloc(len);

	do {
		dp = opendir(path);
		time_t oldest_time = 0;
		char oldest_name[max_filename];
		if (dp == NULL) {
			status = -errno;
			goto cleanup;
		}

		num_cores = 0;
		do {
			struct stat s;

			status = readdir_r(dp, entry, &de);
			if (status != 0) {
				status = -errno;
				goto cleanup;
			} else if (de == NULL)
				break;
			else if (de->d_type != DT_REG)
				continue;

			status = stat(de->d_name, &s);
			if (status != 0) {
				status = -errno;
				goto cleanup;
			}

			if (oldest_time == 0 || s.st_atime < oldest_time) {
				oldest_time = s.st_atime;
				strncpy(oldest_name, de->d_name, len);
			}
			num_cores++;
		} while (1);

		if (num_cores >= toilet_conf.max_files) {
			status = unlink(oldest_name);
			if (status != 0) {
				status = -errno;
				goto cleanup;
			}
			num_cores--;
		}
	} while (num_cores >= toilet_conf.max_files);
cleanup:
	closedir(dp);
	free(entry);
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
	if (status != 0)
		return status;

	status = toilet_flush_cores(".");
	if (status != 0) {
		toilet_preclose(path, 1);
		return status;
	}

	fd = creat(path, mode);
	if (fd < 0) {
		toilet_preclose(path, 1);
		status = -errno;
	} else
		fi->fh = fd;
	return status;
}

static int toilet_release(const char *path, struct fuse_file_info *fi)
{
	FIX_PATH(path);

	toilet_preclose(path, 0);
	/*
	 * Call the hook before the file closes. That way the segfaulting
	 * process should stay open and we can do some process introspection
	 * in the script.
	 */
	close(fi->fh);
	return 0;
}


static int toilet_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi)
{
	(void)path;

	int s = pread(fi->fh, buf, size, offset);
	if (s == -1)
		return -errno;

	return s;
}

static int toilet_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	(void)path;

	int s = pwrite(fi->fh, buf, size, offset);
	if (s == -1)
		return -errno;

	return s;
}

static int toilet_truncate(const char *path, off_t size)
{
	int status = 0;

	FIX_PATH(path);
	pthread_mutex_lock(&lock);
	if (open_count > 0 && strcmp(path, opened_filename) != 0)
		status = -EACCES;
	pthread_mutex_unlock(&lock);

	if (status == 0) {
		status = truncate(path, size);
		if (status == -1)
			return -errno;
	}

	return status;
}

static int toilet_unlink(const char *path)
{
	FIX_PATH(path);
	if (unlink(path) != 0)
		return -errno;
	return 0;
}

static void *toilet_init(struct fuse_conn_info *conn)
{
	if (chdir(toilet_conf.backing_dir) != 0) {
		perror("Failed to change working directory");
		exit(1);
	}

	return NULL;
}

static struct fuse_operations toilet_oper = {
	.getattr	= toilet_getattr,
	.readdir	= toilet_readdir,
	.open		= toilet_open,
	.create		= toilet_create,
	.release	= toilet_release,
	.unlink		= toilet_unlink,
	.read		= toilet_read,
	.write		= toilet_write,
	.truncate	= toilet_truncate,
	.init		= toilet_init,
};

static struct fuse_opt toilet_opts[] =
{
	{ "backing_dir=%s", offsetof(typeof(toilet_conf), backing_dir), 0 },
	{ "close_hook=%s", offsetof(typeof(toilet_conf), close_hook), 0 },
	{ "max_files=%d", offsetof(typeof(toilet_conf), max_files), 0 },
	FUSE_OPT_END
};

int main(int argc, char *argv[])
{
	int status;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (fuse_opt_parse(&args, &toilet_conf, toilet_opts, NULL) == -1)
		return 1;

	if (!toilet_conf.backing_dir) {
		fprintf(stderr, "Need to specify backing_dir mount option!\n");
		return 1;
	}

	if (toilet_conf.max_files == 0)
		toilet_conf.max_files = 5;

	if (pthread_mutex_init(&lock, NULL) != 0) {
		perror("Failed to init mutex");
		return errno;
	}

	status = fuse_main(args.argc, args.argv, &toilet_oper, NULL);
	pthread_mutex_destroy(&lock);
	return status;
}
