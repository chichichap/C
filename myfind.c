/* 
 * csc209 a2p3 by Charuvit Wannissorn (1000149341) 
 * Command-line Input: one or more directories
 * Options: [-f] directories are not output
 *			[-d] maximum traversal depth
 *			[-s] minimum file size to be shown
 *			[-m] maximum file age in days to be shown
 * Output: path of each entry in the directories
 * Description: similar to unix command "find"
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h> /* for readdir() */
#include <sys/types.h>
#include <sys/stat.h> /* for lstat() */
#include <unistd.h>  /* for getopt() */
#include <stdlib.h> /* for atoi() */
#include <time.h> /* for difftime() */

int fFlag = 0, mFlag = 0, maxDepth = 32767, currentDepth = 0, minSize = 0, maxAge, currentPathSize;
char *nextPath;

int main(int argc, char **argv) {
	int i, c, status = 0;
	extern void traverse(char *currentPath);

	while ((c = getopt(argc, argv, "fd:s:m:")) != EOF) {
		switch (c) {
			case 'f':	fFlag = 1;
						break;
			case 'd':	maxDepth = atoi(optarg);
						break;
			case 's':	minSize = atoi(optarg);
						break;
			case 'm':	mFlag = 1;
						maxAge = atoi(optarg);
						break;
			case '?':
			default:	status = 1;
						break;
		}
	}

	/* print a usage message if user type in invalid option(s) or no filename */
	if (status || optind >= argc) {
		fprintf(stderr, "usage: %s [-f] [-d MaxDepth] [-s ByteSize] [-m mTimeDays] dir ...\n", argv[0]);
		return(status);
    }

	/* traverse each input directory */
	for (i=optind; i < argc; i++) {
		currentDepth = 0;
		currentPathSize = 0;
		traverse(argv[i]);
	}
    
    return(0);
}

void traverse(char *currentPath) {
	DIR *dp;
    struct dirent *p;
	struct stat statbuf;
	int nextPathSize;
	char *end; /* for saving currentPath */

	if ((dp = opendir(currentPath)) == NULL) {
		perror(currentPath);
		exit(1);
    }
	
	while ((p = readdir(dp))) {

		/* update currentPath */
		nextPathSize = strlen(currentPath) + 1 + strlen(p->d_name);

		if (currentPathSize < nextPathSize) {
			currentPathSize = nextPathSize;

			/* Memory Reallocation */
			if ((nextPath = realloc(NULL, nextPathSize)) == NULL) {
				fprintf(stderr, "out of memory!\n");
				exit(1);
			}
		}

		/* save currentPath since it might be pointing to some nextPath and thus modified*/
		end = strchr(currentPath, '\0');

		/* nextPath is always properly memory-allocated, so we can modify the string */
		sprintf(nextPath, "%s/%s", currentPath, p->d_name);

		/* call lstat() to get information of the directory entry */
		if (lstat(nextPath, &statbuf)) {
			perror(nextPath);
			exit(1);
		}

		/* BASECASE: print plain file OR symlink */
		/* BASECASE: skip current directory (.) AND parent directory (..) */
		/* ELSE: print (if no -f option) and traverse (if within maxDepth) the directory */
		if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {

			if (statbuf.st_size >= minSize && (!mFlag || difftime(time(NULL), statbuf.st_mtime) <= maxAge*24*60*60))
				printf("%s\n", nextPath);

		} else if (S_ISDIR(statbuf.st_mode) && strcmp(p->d_name, ".") && strcmp(p->d_name, "..")) {

			if (fFlag == 0 && (!mFlag || difftime(time(NULL), statbuf.st_mtime) <= maxAge*24*60*60))
				printf("%s\n", nextPath);

			if (currentDepth <= maxDepth) {
				currentDepth++;
				traverse(nextPath);
				currentDepth--; /* get currentDepth back */
			}
		}

		*end = '\0'; /* get curretPath back */
	}
	closedir(dp);
}