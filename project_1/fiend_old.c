#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <fnmatch.h>
#include <errno.h>
#include "fiend.h"


/*0 = name, 1 = newer, 2 = print, 3 = exec*/
enum expnames {name, newer, print, exec};


struct exp {
	int id;
	char *data;
};

struct inodes {
	int* list;
	int cur_index;
	int alloc_length;
};

struct inodes {
	int* list;
	int cur_index;
	int alloc_length;
};

int recursion(char* directory, int level, long maxdepth, struct exp** expressions, int* exprowlengths, int expnumrows, bool dfs, bool symbolic, struct inodes* my_inodes);

int processfile(const char* file, struct exp** expressions, int* exprowlengths, int expnumrows, bool symbolic);


int main(int argc, char** argv)
{
	/*if(argc<2){
		fprintf(stderr, "Usage: ./fiend [-P|-L]* FILENAME* EXPRESSION\n");
	}
	*/

	bool symbolic = false;

	bool dfs = false;

	long maxdepth = -1;

	long testdepth;

	/* ALLOCATE STORAGE FOR FILENAMES ARRAY */
	char **filenames;

	int numfiles = 0;

	int numfilesalloc = 2;

	filenames = malloc(numfilesalloc*sizeof(char*));
	/*End filename allocation*/

	/* ALLOCATE STORAGE FOR EXPRESSIONS ARRAY */
	struct exp **expressions;

	int *exprowlengths;

	int *alloc_exprowlengths;

	int curlength;
	int newlength;

	exprowlengths = malloc(2*sizeof(int));
	exprowlengths[0] = 0;
	exprowlengths[1] = 0;

	alloc_exprowlengths = malloc(2*sizeof(int));
	alloc_exprowlengths[0] = 2;
	alloc_exprowlengths[1] = 2;

	int alloc_expnumrows = 2;

	expressions = malloc(alloc_expnumrows*sizeof(struct exp*));
	
	for(int i=0; i<alloc_expnumrows; i++){
		expressions[i] = malloc(alloc_exprowlengths[i]*sizeof(struct exp));
	}
	/*End expression allocation*/

	bool actiongiven = false;

	int cur_row = 0;

	int section = 1;

	bool toolate = false; /*true when first test, action, or operator has been read*/

	char* next; /*part of code attribution  in -maxdepth section*/

	char *exec_string;

	struct stat filestat;

	for(int i=1; i < argc; i++){
		if(section == 1){
			if(strcmp("-P", argv[i]) ==0){
				symbolic = false;
			}

			else if(strcmp("-L", argv[i]) ==0){
				symbolic = true;
			}

			else{
				i--;
				section = 2;
			}
		}

		else if(section == 2){
			
			/*printf("made it to sec 2: %s\n", argv[i]);*/

			if(argv[i][0] == '-'){
				i--;

				section = 3;

				if(numfiles == 0){

					filenames[numfiles] = ".";

					numfiles = 1;
				}
			}

			else{
				
				numfiles++;

				if(numfiles > numfilesalloc){

					numfilesalloc = numfilesalloc*2;

					filenames = realloc(filenames, numfilesalloc*sizeof(char*));
				}

				/*filenames[numfiles] = malloc((strlen(argv[i])+1)*sizeof(char));*/

				filenames[numfiles-1] = argv[i];

			}
		}

		else if(section == 3){
			if(strcmp("-depth", argv[i])==0){
				dfs = true;

				if(toolate){
					fprintf(stderr, "Warning: -depth should precede tests, actions, and operators\n");
				}
			}

			else if(strcmp("-maxdepth", argv[i])==0){

				if(i == argc-1){
					fprintf(stderr, "Usage: -maxdepth must be followed by an integer\n");
					exit(1);
				}

				i++;

				/*TODO: "FIEND MUST DEAL WITH ERRNO WHEN CALLING STRTOUL"*/

				/*Attribution: following lines from paxdiablo on Stack Overflow*/
				/*https://stackoverflow.com/questions/17292545/how-to-check-if-the-input-is-a-number-or-not-in-c*/
				errno = 0;

				testdepth = strtol(argv[i], &next, 10);

				if((errno != 0) || (next==argv[i]) || (*next != '\0') || testdepth < 0){
					fprintf(stderr, "Usage: -maxdepth must be followed by a positive integer\n");
					exit(1);
				}

				else{
					maxdepth = testdepth;
				}
				/*Attribution ends here*/

				if(toolate){
					fprintf(stderr, "Warning: -maxdepth should precede tests, actions, and operators\n");
				}
			}

			else if(strcmp("-name", argv[i])==0){
				
				if(i == argc-1){
					fprintf(stderr, "Usage: -name must be followed by a file\n");
					exit(1);
				}

				toolate = true;

				expressions[cur_row][exprowlengths[cur_row]].id = name;

				i++;

				expressions[cur_row][exprowlengths[cur_row]].data = strdup(argv[i]);

				exprowlengths[cur_row]++;

			}

			else if(strcmp("-newer", argv[i])==0){

				if(i == argc-1){
					fprintf(stderr, "Usage: -newer must be followed by a file\n");
					exit(1);
				}

				toolate = true;

				expressions[cur_row][exprowlengths[cur_row]].id = newer;

				i++;

				if(stat(argv[i], &filestat) == -1){
					fprintf(stderr, "Warning: invalid filename '%s'\n", argv[i]);
					exit(1);
				}

				else{
					expressions[cur_row][exprowlengths[cur_row]].data = strdup(argv[i]);

					exprowlengths[cur_row]++;
				}
			}

			else if(strcmp("-print", argv[i])==0){
				toolate = true;
				actiongiven = true;

				expressions[cur_row][exprowlengths[cur_row]].id = print;

				expressions[cur_row][exprowlengths[cur_row]].data = NULL;

				exprowlengths[cur_row]++;
			}

			else if(strcmp("-exec", argv[i])==0){

				if(i == argc-1){
					fprintf(stderr, "Usage: -exec must be followed by a command\n");
					exit(1);
				}

				toolate = true;
				actiongiven = true;

				expressions[cur_row][exprowlengths[cur_row]].id = exec;

				int alloc_exec_string = 4;				

				exec_string = malloc(alloc_exec_string*sizeof(char));

				int length_exec_string = 0;

				i++;

				if(strchr(argv[i], ';') != NULL){
					fprintf(stderr, "Usage: -exec must be followed by a command\n");
					exit(1);
				}

				bool first_entry = true;

				while((i < argc) && (strchr(argv[i], ';') == NULL)){

					while((length_exec_string+strlen(argv[i])+2)>= alloc_exec_string){
						
						alloc_exec_string = 2*alloc_exec_string;

						exec_string = realloc(exec_string, alloc_exec_string*sizeof(char));
					}

					if(first_entry){
						strcpy(exec_string, argv[i]);
					}

					else{
						strcat(exec_string, " ");

						strcat(exec_string, argv[i]);
					}

					length_exec_string += strlen(argv[i]);

					i++;

					first_entry = false;
				}

				expressions[cur_row][exprowlengths[cur_row]].data = exec_string;

				exprowlengths[cur_row]++;
			}

			else if(strcmp("-o", argv[i])==0){
				toolate = true;

				cur_row++;

				if(cur_row > alloc_expnumrows){

					curlength = alloc_expnumrows;

					newlength = 2*curlength;

					alloc_exprowlengths = realloc(alloc_exprowlengths, newlength*sizeof(int));

					exprowlengths = realloc(exprowlengths, newlength*sizeof(int));

					expressions = realloc(expressions, newlength*sizeof(struct exp*));
					
					for(int i=curlength; i<newlength; i++){
						alloc_exprowlengths[i] = 2;
						
						exprowlengths[i] = 0;
						
						expressions[i] = malloc(alloc_exprowlengths[i]*sizeof(struct exp));
						
					}
				}

				if(strcmp("-o", argv[i+1])==0 || strcmp("-a", argv[i+1])==0){
					fprintf(stderr, "Usage: '%s' may not follow '-o'\n", argv[i+1]);
					exit(1);
				}
			}

			else if(strcmp("-a", argv[i])==0){
				toolate = true;

				if(strcmp("-o", argv[i+1])==0 || strcmp("-a", argv[i+1])==0){
					fprintf(stderr, "Usage: '%s' may not follow '-a'\n", argv[i+1]);
					exit(1);
				}

			}

			else{
				fprintf(stderr, "Usage: ./fiend [-P|-L]* FILENAME* EXPRESSION\n");
				exit(1);
			}

			if(exprowlengths[cur_row]+1 > alloc_exprowlengths[cur_row]){

				alloc_exprowlengths[cur_row] = alloc_exprowlengths[cur_row]*2;

				expressions[cur_row] = realloc(expressions[cur_row], alloc_exprowlengths[cur_row]*sizeof(struct exp));
			}
		}
	}

	if(argc == 1){

		filenames[numfiles] = ".";

		numfiles = 1;
	}


	if(!actiongiven){
		
		expressions[cur_row][exprowlengths[cur_row]].id = print;

		expressions[cur_row][exprowlengths[cur_row]].data = NULL;

		exprowlengths[cur_row]++;
	}

	int level;


	struct inodes* my_inodes = malloc(sizeof(struct inodes));

	/*printf("made it here\n");*/

	for(int i = 0; i<numfiles; i++){

		level = 0;

		my_inodes->cur_index = 0;

		my_inodes->alloc_length = 2;

		my_inodes->list = malloc(my_inodes->alloc_length*sizeof(int));

		recursion(filenames[i], level, maxdepth, expressions, exprowlengths, cur_row + 1, dfs, symbolic, my_inodes);

		free(my_inodes->list);

	}

	for(int i=0; i<alloc_expnumrows; i++){

		for(int j=0; j < exprowlengths[i]; j++){

			free(expressions[i][j].data);
		}

		free(expressions[i]);
	}

	free(expressions);
	free(exprowlengths);
	free(alloc_exprowlengths);
	free(filenames);
	free(my_inodes);
	
	return 0;
}


int recursion(char* directory, int level, long maxdepth, struct exp** expressions, int* exprowlengths, int expnumrows, bool dfs, bool symbolic, struct inodes* my_inodes){
	
	/*printf("running recursion: %s\n", directory);*/

	/*char *linkedfile;*/

	struct stat filestat;

	struct stat linkstat;

	DIR *dir;

	bool go_further = false;

	bool tryagain = false;

	errno = 0;

	/*TODO: MAKE SEPARATE INODE LIST FOR EACH RECURSION PATH*/

	if(symbolic){
		
		if(stat(directory, &filestat)==-1){
			if(errno == 40){ /*ELOOP when calling stat*/
				fprintf(stderr, "symlink loop detected\n");
				return 1;
			}
			if(errno == 14){
				tryagain = true;
			}
			else if(errno != 14){
				fprintf(stderr, "invalid filename: %s\n", directory);
				return 1;
			}
		}

		else{
			if(S_ISDIR(filestat.st_mode)){
				go_further = true;
			}

			lstat(directory, &linkstat);

			if(S_ISLNK(linkstat.st_mode)){

				printf("it's a link: %s\n",directory);

				int link_inode = linkstat.st_ino;

				int file_inode = filestat.st_ino;

				for(int i=0; i<my_inodes->cur_index; i++){
					if(file_inode == my_inodes->list[i] || link_inode == my_inodes->list[i]){
						fprintf(stderr, "symlink loop detected\n");
						return 1;
					}
				}

				if(my_inodes->cur_index >= my_inodes->alloc_length){
					my_inodes->alloc_length = 2*my_inodes->alloc_length;
					my_inodes->list = realloc(my_inodes->list, my_inodes->alloc_length*sizeof(int));
				}

				my_inodes->list[my_inodes->cur_index] = link_inode;
				my_inodes->cur_index++;
			}
		}
	}

	errno = 0;

	if(!symbolic || tryagain){
		
		if(lstat(directory, &filestat) == -1){
			fprintf(stderr, "invalid filename: %s\n", directory);
			return 1;
		}

		else{
			if(S_ISDIR(filestat.st_mode)){
				go_further = true;
			}
		}
	}

	/*
	int cur_inode = filestat.st_ino;

	for(int i=0; i<my_inodes->cur_index; i++){
		if(cur_inode == my_inodes->list[i]){
			fprintf(stderr, "symlink loop detected\n");
			return 1;
		}
	}

	if(my_inodes->cur_index >= my_inodes->alloc_length){
		my_inodes->alloc_length = 2*my_inodes->alloc_length;
		my_inodes->list = realloc(my_inodes->list, my_inodes->alloc_length*sizeof(int));
	}

	my_inodes->list[my_inodes->cur_index] = cur_inode;
	my_inodes->cur_index++;
	*/

	if(!dfs){
		processfile(directory, expressions, exprowlengths, expnumrows, symbolic);
	}

	if(go_further && (maxdepth ==-1 || level+1 <= maxdepth)){

		dir = opendir(directory);

		struct dirent *entry;

		if(dir == NULL){
			printf("oops\n");
			/*TODO: CHECK ERRNO*/
		}

		else{
			while((entry = readdir(dir)) != NULL){			

				if(!(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0)){

					/*printf("still going\n");*/

					char* full_filename = malloc((strlen(directory)+strlen(entry->d_name)+2)*sizeof(char));

					strcpy(full_filename, directory);

					if(full_filename[strlen(directory)-1] != '/'){
						strcat(full_filename,  "/");
					}

					strcat(full_filename, entry->d_name);

					/*if(entry->d_type == DT_DIR){
			
						recursion(full_filename, level+1, maxdepth, expressions, exprowlengths, expnumrows, dfs, symbolic);
						
					}*/

					/*if(entry->d_type == DT_LNK && symbolic){*/
						
						/*TODO: check for symbolic link loop*/
						
						/*linkedfile = realpath(entry->d_name, NULL);*/
						
						/*TODO: free linkedfile*/

						/*TODO: deal with filenames of symlinks*/
						
						/*stat(linkedfile, &filestat);

						if(S_ISDIR(filestat.st_mode)){

							recursion(linkedfile, level+1, maxdepth, expressions, exprowlengths, expnumrows, dfs, symbolic);
						}

						else{
							processfile(linkedfile, expressions, exprowlengths, expnumrows, symbolic);
						}

					}*/

					recursion(full_filename, level+1, maxdepth, expressions, exprowlengths, expnumrows, dfs, symbolic, my_inodes);

					free(full_filename);
				}
			}
		}

		closedir(dir);
	}

	if(dfs){
		processfile(directory, expressions, exprowlengths, expnumrows, symbolic);
	}


	return 0;
}

int processfile(const char* file, struct exp** expressions, int* exprowlengths, int expnumrows, bool symbolic){

	char* cur_file = strdup(file);

	bool logic;

	struct exp cur_exp;

	struct stat status;

	struct stat comp_status;

	/*printf("running processfile: %s\n", cur_file);*/

	/*char* full_name = realpath(cur_file, NULL);*/

	/*printf("%s\n", full_name);*/

	if(symbolic){
		stat(cur_file, &status);
	}

	else{
		lstat(cur_file, &status);
	}

	/*char *basename = dirname(cur_file);*/


	for(int i = 0; i < expnumrows; i++){
		
		logic = false;

		for(int j = 0; j < exprowlengths[i]; j++){

			cur_exp = expressions[i][j];

			if(cur_exp.id == name){

				char*file_suffix = strdup(cur_file);

				if(fnmatch(cur_exp.data, basename(file_suffix), 0) == 0){
					logic = true;
				}
				else{
					logic = false;
				}

				free(file_suffix);
			}

			else if(cur_exp.id == newer){
				
				if(symbolic){
					stat(cur_exp.data, &comp_status);
				}
				else{
					lstat(cur_exp.data, &comp_status);
				}


				if(status.st_mtim.tv_nsec > comp_status.st_mtim.tv_nsec){
					logic = true;
				}
				else{
					logic = false;
				}

			}

			else if(cur_exp.id == print){

				printf("%s\n", cur_file);

				logic = true;
			}

			else if(cur_exp.id == exec){

				int alloc_command = strlen(cur_exp.data) + 1;

				int length_command = 0;

				char *command = malloc(alloc_command*sizeof(char));

				int recent_paren = 0;
				
				for(int k = 0; k < strlen(cur_exp.data)-1; k++){

					if((cur_exp.data[k] == '{') && (cur_exp.data[k+1] == '}')){

						while((length_command + strlen(cur_file) + k - recent_paren + 1) >= alloc_command){
							alloc_command = 2*alloc_command;
							command = realloc(command, alloc_command*sizeof(char));
						}

						if(length_command == 0){

							if(k - recent_paren == 0){
								strcpy(command, cur_file);
							}

							else{

								int l;

								for(l=recent_paren; l < k; l++){
									command[l] = cur_exp.data[l];
								}

								command[l] = '\0';
																
								strcat(command, cur_file);
							}	
						}

						else{
							strncat(command, cur_exp.data + recent_paren, k-recent_paren);

							strcat(command, cur_file);
						}

						length_command += strlen(cur_file) + k - recent_paren;

						recent_paren = k+2;
					}
				}

				if(recent_paren == 0){

					strcpy(command, cur_exp.data);
				}

				else{
					while((length_command + strlen(cur_exp.data) - recent_paren) >= alloc_command){
						alloc_command = 2*alloc_command;
						command = realloc(command, alloc_command*sizeof(char));
					}

					strncat(command, cur_exp.data + recent_paren, strlen(cur_exp.data)-recent_paren);
				}	

				fflush(stdout);

				/*printf("command: %s\n", command);*/

				if(system(command) == 0){
					logic = true;
					/*printf("yes\n");*/
				}
				else{
					logic = false;
				}

				free(command);
			}

			if(!logic){
				break;
			}
		}

		if(logic){
			break;
		}
	}

	/*free(full_name);*/

	free(cur_file);

	return 0;

}


/*TODO: check errno for all system calls (see spec)*/

/*TODO: keep track of lengths to replace length calls*/

/*TODO: free memory*/

/*TODO: symbolic link loop*/

/*TODO: string replacement for -exec*/

/*TODO: -exec command with {{}{}{}{}{}{}*/