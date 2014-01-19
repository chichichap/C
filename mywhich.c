#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
	DIR *dp;
    struct dirent *p;
	struct stat statbuf;
	int i, j, found;
	char *SearchList[3] = {"/bin", "/usr/bin", "."};
	char filePath[1000];

	if (argc < 2) {
		printf("usage: %s commandname ... \n", argv[0]);
		return(1);
	}

	/* look for all commands */
    for (i=1; i < argc; i++) {
		found = 0;

		/* look in the three directories */
		for (j=0; j < 3; j++) 
		{
			if ((dp = opendir(SearchList[j])) == NULL) {
				perror(SearchList[j]);
				return(1);
			}

			while ((p = readdir(dp))) 
			{
				if (strcmp(argv[i], p->d_name) == 0) {
					/* print error message if filepath is 1000 bytes or longer*/
					if (strlen(SearchList[j]) + 1 + strlen(argv[i]) > 999) {
						fprintf(stderr, "Error: filepath has to be less than 1000 characters");
						return(1);
					}

					/* make the filepath */
					sprintf(filePath, "%s/%s", SearchList[j], argv[i]);

					/* call stat() to get st_mode of the file */
					if (stat(filePath, &statbuf)) {
						perror(filePath);
						return(1);
					} 
					
					/* the file is found if it is executable by owner */
					if (statbuf.st_mode & 0100) {
						printf("%s/%s\n", SearchList[j], p->d_name);
						found = 1; 
						break;
					}
				}
			}

			closedir(dp);
			if (found)	break;
		}

		if (!found)	printf("%s: Command not found.\n", argv[i]);
	}
    
    return(0);
}