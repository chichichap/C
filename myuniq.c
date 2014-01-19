/* 
 * csc209 a2p2 by Charuvit Wannissorn (1000149341) 
 * Command-line Input: one or more filenames (e.g. test.txt)
 * Options: [-c] show the number of adjacent identical line(s)
 * Output: filepath of the executable program
 * Description: similar to unix command "uniq"
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>  /* for getopt() */

int main(int argc, char **argv) {
	int i, c, count, cFlag = 0, status = 0;
	char lineBufferA[500], lineBufferB[500];
	FILE *fp;

	while ((c = getopt(argc, argv, "c")) != EOF) {
		switch (c) {
		case 'c':
			cFlag = 1;
			break;
		case '?':
		default:
			status = 1;
			break;
		}
	}
	
	/* print a usage message if user type in invalid option(s) or no filename */
	if (status || optind >= argc) {
		fprintf(stderr, "usage: %s [-c] [name ...]\n", argv[0]);
		return(status);
    }

	/* for each filename */
	for (i=optind; i < argc; i++) {

		/* print an error message if fail to open the file */
		if ((fp = fopen(argv[i], "r")) == NULL) {
			perror(argv[i]);
			status = 1;
			continue;
		}

		/* use two alternating lineBuffers to find identical adjacent lines */
		if (fgets(lineBufferA, 500, fp) != NULL) {
			count = 1;

			/* find next unidentical line = lineBufferB */
			while (fgets(lineBufferB, 500, fp) != NULL) {
				if (strcmp(lineBufferA, lineBufferB) == 0) count++;
				else {
					/* print each unique line once */
					if (cFlag) printf("\t%d %s", count, lineBufferA);
					else printf("%s", lineBufferA);
					
					/* lineBufferA = next unidentical line */
					strcpy(lineBufferA, lineBufferB);
					count = 1;
				}
			}

			/* print the last unique line once */
			if (cFlag) printf("\t%d %s\n", count, lineBufferA);
			else printf("%s\n", lineBufferA);
		}
		fclose(fp);
	} 
    return(status);
}