/*
  toiletfs: the one-at-a-time filesystem.
  Copyright (C) 2012  Scott J. Goldman <scottjg@github.com>

  This program can be distributed under the terms of the MIT License.
  See the file COPYING.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>

void test_no_concurrent_opens()
{
	FILE *fp, *fp1, *bad_fp;

	fp = fopen("test", "w");
	assert(fp);

	bad_fp = fopen("nope", "w");
	assert(!bad_fp);

	fp1 = fopen("test", "r");
	assert(fp1);
	fclose(fp1);

	fclose(fp);
	fp = fopen("test again", "w");
	assert(fp);
	fclose(fp);

	unlink("test again");
	unlink("test");
}

void test_no_concurrent_coredumps()
{
	int i, status, dumps = 0;
	FILE *fp;
	char buf[128];
	int len;

	for (i=0; i < 5; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			struct rlimit limit = {-1, -1};
			int i;
			char *mem;

			assert(setrlimit(RLIMIT_CORE, &limit) == 0);
			for (i=0; i < 25; i++) {
				mem = malloc(1024*1024);
				memset(mem, 1, 1024*1024);
				assert(mem);
			}

			*(char *)0 = 0;
			_exit(0);
		}
	}

	dumps = 0;
	while (errno != ECHILD) {
		pid_t pid = wait(&status);
		if (pid < 0)
			continue;
		else if (WIFSIGNALED(status) && WCOREDUMP(status))
			dumps++;
	}
	assert(dumps == 1);

	//XXX need to unlink coredump
}

void test_rotation()
{
	int i;
	char file[32];
	struct stat s;
	for (i = 0; i < 7; i++) {
		FILE *fp;
		sprintf(file, "test%d", i);
		fp = fopen(file, "w");
		fclose(fp);
		sleep(1);
	}

	assert(stat("test0", &s) != 0);
	assert(stat("test1", &s) != 0);

	for (i = 2; i < 7; i++) {
		sprintf(file, "test%d", i);
		assert(stat(file, &s) == 0);
		unlink(file);
	}
}

int main(int argc, char **argv)
{
	char *test_mount = argv[1];

	if (argc < 2) {
		fprintf(stderr, "Need to specify a mount to test!\n");
		exit(1);
	}

	if (chdir(test_mount) != 0) {
		perror("failed to chdir");
		exit(1);
	}

	printf("Testing...\n");
	test_no_concurrent_opens();
	test_no_concurrent_coredumps();
	test_rotation();
	printf("All tests passed!\n");

	return 0;
}
