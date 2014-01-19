/* 
 * csc209 a3p1 by Charuvit Wannissorn (1000149341) 
 * Input: filenames as command-line argument
 *	      (stdin if zero filename argument)
 * Output: a randome byte (character) for each input file
 */

#include <stdio.h>
#include <sys/stat.h> /* for lstat() */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv) {
	int i, fd;
    extern void process(int fd, char *filename);

	/* zero filename = read from stdin */
	if (argc == 1)	process(0, "stdin");
	else {
			/* for each filename */
			for (i = 1; i < argc; i++) 
			{
				/* open the file */
				if ((fd = open(argv[i], O_RDONLY)) < 0) {
					perror(argv[i]);
					return(1);
				} 
				
				process(fd, argv[i]);
				close(fd);
			}
		}

    return(0);
}

void process(int fd, char *filename)
{
	unsigned long x;
    int fd2, error;
	char c;
	struct stat statbuf;

	/* open /dev/urandom */
	if ((fd2 = open("/dev/urandom", O_RDONLY)) < 0) {
		perror("/dev/urandom");
		exit(1);
	}

	/* read a random unsigned long */
	error = read(fd2, &x, sizeof(unsigned long));
	if (error < 0) {
		perror("read");
		exit(1);
	} 
	
	close(fd2);

	/* get file size */
	if(fstat(fd, &statbuf) < 0) {
		perror(filename); 
		exit(1);
	}

	/* if file size = 0 (contains nothing), print nothing */
	if (statbuf.st_size == 0) return;

	/* set reading pointer to the random byte position*/
	if (lseek(fd, x % statbuf.st_size, SEEK_SET) < 0) {
		perror(filename);
		exit(1);
	}

	/* read the random byte */
	error = read(fd, &c, 1);

	/* error = 0 means EOF, print nothing*/
	if (error > 0) putchar(c);
	else if (error < 0) {
		perror("read");
		exit(1);
	} 
}
