/* 
 * csc209 a3p2 by Charuvit Wannissorn (1000149341) 
 * Input: a directory name as command-line argument
 * Output: Nothing (except error message if there's any)
 * Description: The program will create the directory in current
 *   directory and create files whose filenames are prime numbers
 *   from 2 to 5000 (GOAL). In detail, the program spawns NPROC 
 *   child processes, and distribute work to each child, so the
 *   job can be done in paralel.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h> /* for readdir() */
#include <sys/types.h>
#include <sys/stat.h>

#define NPROC 5
#define GOAL 5000

int AssignmentPipe[NPROC], DonePipe[NPROC];

extern void doit(int from, int to);
extern void spawnall(), be_master();
extern void be_child(int fd_from_master, int fd_to_master);
extern void be_master(int DoneSoFar);

/* added functions */
extern void createFile(int number);
extern void writeTo(int fd, int* addr);
extern int readFrom(int fd, int* addr);

int main(int argc, char **argv) {
    if (argc != 2) {
		fprintf(stderr, "usage: %s dir\n", argv[0]);
		return(1);
    }

    if (mkdir(argv[1], 0777)) {
		perror(argv[1]);
		return(1);
    }

    if (chdir(argv[1])) {
		perror(argv[1]);
		return(1);
    }
	
    doit(2, 10);
    spawnall();
	be_master(10);

    return(0);
}

void spawnall() {
	int Apipefd[NPROC][2], Dpipefd[NPROC][2];
	int i, pid;

	for (i=0; i < NPROC; i++) {

		if (pipe(Apipefd[i])) {
			perror("pipe");
			exit(1);
		}

		if (pipe(Dpipefd[i])) {
			perror("pipe");
			exit(1);
		}

		pid = fork();

		if (pid < 0) {
			perror("fork");
			exit(1);
		} else if (pid == 0) {
			close(Dpipefd[i][0]);
			close(Apipefd[i][1]);
			
			be_child(Apipefd[i][0], Dpipefd[i][1]);
		}  
		
		close(Dpipefd[i][1]);
		close(Apipefd[i][0]);

		/* parent cannot read here or it will wait*/
		DonePipe[i] = Dpipefd[i][0];		/* will read later */
		AssignmentPipe[i] = Apipefd[i][1];  /* will write later */
	}
}

/* does not return */
void be_child(int fd_from_master, int fd_to_master) { 
	int start = 0, end = 0;

	while (1) {
		writeTo(fd_to_master, &start);
		writeTo(fd_to_master, &end);

		/* got EOF from master = exit */
		if (readFrom(fd_from_master, &start) == 0) exit(0);
		if (readFrom(fd_from_master, &end) == 0) exit(0);

		doit(start, end);
	}
}


void be_master(int DoneSoFar) {
	int i, start, end, rangeSize, lastNumberAssigned = DoneSoFar;

	/* for 11 to 100, we want the rangeSize to be equal */
	rangeSize = (DoneSoFar*DoneSoFar - DoneSoFar) / NPROC;

	while(1) {
		for (i=0; i < NPROC; i++) {
			readFrom(DonePipe[i], &start);
			readFrom(DonePipe[i], &end);

			if (end > 0) {
				printf("child #%d report completion to %d\n", i, end);
				
				/* check that no number is missed */
				if (start != DoneSoFar+1)
					fprintf(stderr, "Error: some number is missed\n");

				DoneSoFar = end;
				rangeSize = (DoneSoFar*DoneSoFar - lastNumberAssigned) / NPROC;
			}

			/* goal reached, exit now */
			if (DoneSoFar >= GOAL) exit(0);

			/* update new ranges to hand out */
			start = lastNumberAssigned + 1;
			end = lastNumberAssigned + rangeSize;
			if (end > GOAL)
				end = GOAL;

			printf("asking child #%d to do from %d to %d\n", i, start, end);

			writeTo(AssignmentPipe[i], &start);
			writeTo(AssignmentPipe[i], &end);

			lastNumberAssigned = end;
		}
	}
}

/* create files whose filenames are prime numbers for the range */
void doit(int from, int to) {
	int i, isPrime;
	DIR *dp;
	struct dirent *p;

	for (i=from; i <= to; i++) {
		isPrime = 1;

		if ((dp = opendir(".")) == NULL) {
			perror(".");
			exit(1);
		}

		/* read files in dir ignoring . and .. */
		while ((p = readdir(dp))) {
			/* divisible by a smaller prime means it's a prime */
			if ((atoi(p->d_name) > 0) && (i % atoi(p->d_name) == 0)) {
				isPrime = 0;
				break;
			}
		}

		if (isPrime) createFile(i);
		closedir(dp);
	}
}

void createFile(int number) {
	int fd;
	char name[32];

	sprintf(name, "%d", number);

	if ((fd = open(name, O_CREAT, 0666)) < 0) {
		perror(name);
		exit(1);
	}

	close(fd);
}

void writeTo(int fd, int* addr) {
	if (write(fd, addr, sizeof(int)) != sizeof(int)) {
		perror("write");
		exit(1);
    }
}

/* returns what read() returns */
int readFrom(int fd, int* addr) {
	int len = read(fd, addr, sizeof(int));

	if (len != sizeof(int)) {
		if (len == 0) return 0; /* EOF */

		perror("read");
		exit(1);
	}
	
	return (len); /* len = 4 */
}
