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
};/*struct to hold expressions and their arguments*/


/*ATTRIBUTION: linked list code from "Notes on Data Structures and Programming Techniques (CPSC 223, Spring 2018)" by James Aspnes*/
struct inodes_list {
	int inode;
	struct inodes_list *parent;
};

typedef struct inodes_list *Stack;

#define STACK_EMPTY (0)

int stackEmpty(const Stack *s);

void stackPush(Stack *s, int value);
/*END ATTRIBUTION*/


int recursion(char* directory, int level, long maxdepth, struct exp** expressions, int* exprowlengths, int expnumrows, bool dfs, bool symbolic, Stack cur_parent);

int processfile(const char* file, struct exp** expressions, int* exprowlengths, int expnumrows, bool symbolic);


int main(int argc, char** argv)
{

	bool symbolic = false; /*to record -P (false) or -L (true). By default -P (false) is used*/

	bool dfs = false; /*to record if -depth is used. If so, dfs = true*/

	long maxdepth = -1; /*to record maxdepth. maxdepth = -1 means no maxdepth*/

	long testdepth; /*helper to read maxdepth*/


	/* ALLOCATE STORAGE FOR FILENAMES ARRAY */
	char **filenames; /*store filenames in a list of strings*/

	int numfiles = 0; /*keep track of the list length*/

	int numfilesalloc = 2; /*keep track of how much space has been allocated*/

	filenames = malloc(numfilesalloc*sizeof(char*)); /*throughout, we assume malloc, realloc calls never fail*/
	/*End filename allocation*/



	/* ALLOCATE STORAGE FOR EXPRESSIONS ARRAY */
	struct exp **expressions; /*store expressions in a double array of expression structs*/
	/*expressions within a row are ones that are separated by -a*/
	/*expressions in different rows are separated by -o*/

	int *exprowlengths; /*keep track of the length of each row*/

	int *alloc_exprowlengths; /*keep track of the alloced length of each row*/

	int curlength; /*use to initialize new rows when realloced*/
	int newlength; 

	exprowlengths = malloc(2*sizeof(int));
	exprowlengths[0] = 0; /*rows are initially empty*/
	exprowlengths[1] = 0;

	alloc_exprowlengths = malloc(2*sizeof(int));
	alloc_exprowlengths[0] = 2; /*rows begin with two entries allocated*/
	alloc_exprowlengths[1] = 2;

	int alloc_expnumrows = 2; /*keep track of the number of rows allocated*/

	expressions = malloc(alloc_expnumrows*sizeof(struct exp*)); /*alloc the rows of expressions*/
	
	for(int i=0; i<alloc_expnumrows; i++){
		expressions[i] = malloc(alloc_exprowlengths[i]*sizeof(struct exp)); /*alloc each row*/
	}
	
	int cur_row = 0; /*index for knowing which row to add to*/

	/*End expression allocation*/


	bool actiongiven = false; /*to check if an action is provided. if not, then -print will be added*/

	/*three sections of command line arguments*/
	/*section 1: -P or -L*/
	/*section 2: filenames*/
	/*section 3: expressions*/
	int section = 1;

	bool toolate = false; /*to check if -depth or -maxdepth are not first in expressions*/

	char* next; /*part of code attribution  in -maxdepth section*/

	char *exec_string; /*to read in the possibly several arguments for the exec command*/

	struct stat filestat; /*to check that a file given to -newer is a real file*/

	/*loop through the command line arguments, beginning with the second one*/
	for(int i=1; i < argc; i++){

		/*begin here to check for -P or -L*/
		if(section == 1){

			/*-P means don't follow symbolic links, symbolic = false*/
			if(strcmp("-P", argv[i]) ==0){
				symbolic = false;
			}

			/*-L means follow symbolic links, symbolic = true*/
			else if(strcmp("-L", argv[i]) ==0){
				symbolic = true;
			}

			/*-P or -L must precede the first filename, if the current argument is not -P or -L we have moved to the next section*/
			else{
				i--; /*decrease index so that we process the same argument again in the next loop*/
				section = 2; /*we're know looking for filenames*/
			}
		}

		/*else if construction means that the for loop skips this the first time section = 2*/
		else if(section == 2){
			
			/*arguments that begin with '-' are expressions, if the current argument is an expression, we are ready for section 3*/
			if(argv[i][0] == '-'){
				
				i--; /*decreae index to account for if else construction of loop*/

				section = 3; /*new section*/

				/*we'll make sure at least one filename exists after processing all command-line arguments*/
			}

			/*if current arg does not begin with '-', it is a filename*/
			else{
				
				numfiles++; /*update number  of files*/

				if(numfiles > numfilesalloc){ /*alloc more space in filenames if necessary*/

					numfilesalloc = numfilesalloc*2; /*we'll always realloc by doubling amount of space*/

					filenames = realloc(filenames, numfilesalloc*sizeof(char*));
				}

				filenames[numfiles-1] = argv[i]; /*numfiles - 1 to account for 0 indexing*/
			}
		}

		else if(section == 3){
			/*here, we'll check if the string is the same as any of the possible expressions*/

			if(strcmp("-depth", argv[i])==0){
				dfs = true; /*dfs records the presence of -depth*/

				/*if -depth appears after a test, action, or operator*/
				if(toolate){
					/*print an error but continue execution*/
					fprintf(stderr, "fiend: -depth should precede tests, actions, and operators\n");
				}
			}

			else if(strcmp("-maxdepth", argv[i])==0){

				/*maxdepth requires an argument, if current index is the final command line argument, mexdepth is missing its argument*/
				if(i == argc-1){
					fprintf(stderr, "fiend: -maxdepth must be followed by an integer\n");
					/*exit execution if this is the case*/
					exit(1);
				}

				/*otherwise, its safe to read the next argv*/
				i++;

				/*Attribution: following lines from paxdiablo on Stack Overflow*/
				/*https://stackoverflow.com/questions/17292545/how-to-check-if-the-input-is-a-number-or-not-in-c*/
				errno = 0;

				testdepth = strtol(argv[i], &next, 10); /*we'll read the maxdepth argument as a long, an check if its less than zero*/

				if((errno != 0) || (next==argv[i]) || (*next != '\0') || testdepth < 0){
					/*if there are any problems with the maxdepth argument, print an error and exit execution*/
					fprintf(stderr, "fiend: -maxdepth must be followed by a positive integer\n");
					exit(1);
				}

				/*if the maxdepth argument is all good, save it in maxdepth*/
				/*maxdepth retains its default -1 value with this construction*/
				else{
					maxdepth = testdepth;
				}
				/*Attribution ends here*/

				/*if -maxdepth appears after a test, action, or operator*/
				if(toolate){
					/*print an error but continue execution*/
					fprintf(stderr, "fiend: -maxdepth should precede tests, actions, and operators\n");
				}
			}

			else if(strcmp("-name", argv[i])==0){
				
				/*check that another argv exists before reading it*/
				if(i == argc-1){
					fprintf(stderr, "fiend: -name must be followed by a file\n");
					exit(1);
				}

				toolate = true; /*to record that a test, action, or operator has been read*/

				/*record name in exp struct (we used enum to make name an int)*/
				expressions[cur_row][exprowlengths[cur_row]].id = name;

				i++; /*to read the argument to -name*/

				expressions[cur_row][exprowlengths[cur_row]].data = strdup(argv[i]); /*we copy the string here to make freeing more simple*/

				exprowlengths[cur_row]++; /*update length of this row*/
			}

			else if(strcmp("-newer", argv[i])==0){

				/*check that another argv exists before reading it*/
				if(i == argc-1){
					fprintf(stderr, "fiend: -newer must be followed by a file\n");
					exit(1);
				}

				toolate = true; /*to record that a test, action, or operator has been read*/

				expressions[cur_row][exprowlengths[cur_row]].id = newer; /*record id in exp struct*/

				/*read next argument*/
				i++;

				/*check that the argument to newer is a valid file*/
				if(stat(argv[i], &filestat) == -1){
					fprintf(stderr, "fiend: Invalid filename '%s'\n", argv[i]);
					/*exit execution if invalid*/
					exit(1);
				}

				/*if valid, record it in the exp struct*/
				else{
					expressions[cur_row][exprowlengths[cur_row]].data = strdup(argv[i]);

					exprowlengths[cur_row]++;
				}
			}

			else if(strcmp("-print", argv[i])==0){
				toolate = true; /*to record that a test, action, or operator has been read*/
				actiongiven = true; /*to record that an action is given*/

				/*record in exp struct*/
				expressions[cur_row][exprowlengths[cur_row]].id = print;

				expressions[cur_row][exprowlengths[cur_row]].data = NULL;

				exprowlengths[cur_row]++;
			}

			else if(strcmp("-exec", argv[i])==0){

				/*check that another argv exists before reading it*/
				if(i == argc-1){
					fprintf(stderr, "fiend: -exec must be followed by a command\n");
					exit(1);
				}

				toolate = true; /*to record that a test, action, or operator has been read*/
				actiongiven = true; /*to record that an action is given*/

				/*record in exp struct*/
				expressions[cur_row][exprowlengths[cur_row]].id = exec;

				int alloc_exec_string = 4; /*keep track of alloced length*/				

				exec_string = malloc(alloc_exec_string*sizeof(char));

				int length_exec_string = 0; /*keep track of actual length*/

				/*read next argv*/
				i++;

				/*if -exec is immediately followed by ';', it's missing its command*/
				if(strcmp(argv[i], ";") == 0){
					fprintf(stderr, "fiend: -exec must be followed by a command\n");
					exit(1);
				}

				bool first_entry = true; /*note whether we are copying to exec_string for the first time*/

				/*read the command line arguments until get to end or ';'*/
				while((i < argc) && strcmp(argv[i], ";") != 0){

					/*if we get to the end of the command line arguments without reading a ";", then there is an error in the exec command*/
					if(i == argc-1 && strcmp(argv[i], ";") != 0){
						fprintf(stderr, "fiend: command to -exec must be concluded with a ';'\n");
						exit(1);
					}

					/*realloc more space to the string if needed*/
					while((length_exec_string+strlen(argv[i])+2)>= alloc_exec_string){
						
						alloc_exec_string = 2*alloc_exec_string;

						exec_string = realloc(exec_string, alloc_exec_string*sizeof(char));
					}

					/*if exec_string is empty we need to use strcpy instead of strcat*/
					if(first_entry){
						strcpy(exec_string, argv[i]);
					}


					else{
						strcat(exec_string, " ");/*add a space between arguments*/

						strcat(exec_string, argv[i]);/*add current argument to command*/
					}

					length_exec_string += strlen(argv[i])+1;/*update length var*/

					i++;/*update command line index*/

					first_entry = false;/*exec_string is no longer null*/
				}

				expressions[cur_row][exprowlengths[cur_row]].data = exec_string; /*store exec_string in exp struct*/

				exprowlengths[cur_row]++; /*update length of current row in exp array*/
			}

			else if(strcmp("-o", argv[i])==0){
				toolate = true; /*to record that a test, action, or operator has been read*/

				/*if the current row doesn't have an action, add print to the current row*/
				if(!actiongiven){
		
					expressions[cur_row][exprowlengths[cur_row]].id = print;

					expressions[cur_row][exprowlengths[cur_row]].data = NULL;

					exprowlengths[cur_row]++;
				}

				actiongiven = false; /*reset the indicator for the new row*/

				cur_row++; /*-o indicates a new row in the exp array*/

				/*make array bigger if needed*/
				if(cur_row > alloc_expnumrows){

					curlength = alloc_expnumrows;

					newlength = 2*curlength;

					alloc_exprowlengths = realloc(alloc_exprowlengths, newlength*sizeof(int));

					exprowlengths = realloc(exprowlengths, newlength*sizeof(int));

					expressions = realloc(expressions, newlength*sizeof(struct exp*));
					
					/*initialize the new rows*/
					for(int i=curlength; i<newlength; i++){
						alloc_exprowlengths[i] = 2;
						
						exprowlengths[i] = 0;
						
						expressions[i] = malloc(alloc_exprowlengths[i]*sizeof(struct exp));
						
					}
				}

				/*cannot have two operators in a row*/
				if(strcmp("-o", argv[i+1])==0 || strcmp("-a", argv[i+1])==0){
					fprintf(stderr, "fiend: '%s' may not follow '-o'\n", argv[i+1]);
					exit(1);
				}
			}

			/*-a is assumed between exp in the same row, thus nothing happens for -a*/
			else if(strcmp("-a", argv[i])==0){
				toolate = true; /*to record that a test, action, or operator has been read*/

				/*cannot have two operators in a row*/
				if(strcmp("-o", argv[i+1])==0 || strcmp("-a", argv[i+1])==0){
					fprintf(stderr, "fiend: '%s' may not follow '-a'\n", argv[i+1]);
					exit(1);
				}
			}

			/*exit if there is something unexpected on command line*/
			else{
				fprintf(stderr, "fiend: Usage ./fiend [-P|-L]* FILENAME* EXPRESSION\n");
				exit(1);
			}

			/*realloc if the cur exp array row is out of additional space*/
			if(exprowlengths[cur_row]+1 > alloc_exprowlengths[cur_row]){

				alloc_exprowlengths[cur_row] = alloc_exprowlengths[cur_row]*2;

				expressions[cur_row] = realloc(expressions[cur_row], alloc_exprowlengths[cur_row]*sizeof(struct exp));
			}
		}
	}

	/*if no filenames given, search current directory*/
	if(numfiles == 0){

		filenames[numfiles] = ".";

		numfiles = 1;
	}


	/*if no action was given on the final row, add the print action */
	if(!actiongiven){
		
		expressions[cur_row][exprowlengths[cur_row]].id = print;

		expressions[cur_row][exprowlengths[cur_row]].data = NULL;

		exprowlengths[cur_row]++;
	}

	int level; /*to keep track of the current depth*/

	/*loop through the filenames*/
	for(int i = 0; i<numfiles; i++){

		level = 0; /*top level is a depth of zero*/

		/*the Stack is used to keep track of the current path down the filetree*/
		/*we'll check the current inode number of a file against all the inode numbers of the files that preceeded it*/
		/*this will detect true symbolic link loops*/
		/*alternate methods may exit if the same file was reached by two valid paths*/
		Stack my_list;

		my_list = STACK_EMPTY; /*denote the end of the linked list*/

		/*use recursion to search the file tree*/
		recursion(filenames[i], level, maxdepth, expressions, exprowlengths, cur_row + 1, dfs, symbolic, my_list);
	}


	/*free memory*/
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
	
	return 0;
}


int recursion(char* directory, int level, long maxdepth, struct exp** expressions, int* exprowlengths, int expnumrows, bool dfs, bool symbolic, Stack cur_parent){
	/*Welcome to recursion, here we'll*/
	/*(1) check if the current file is a directory (using lstat or stat)*/
	/*(2) detect symbolic loops with the inode linked list*/
	/*(3) process the current file (either before or after searching its children if its a directory)*/
	/*(4) if the current file is a directory (step 1) we'll search through the directory, passing each file to recursion*/

	struct stat filestat;

	DIR *dir;

	bool go_further = false; /*indicates if cur file is a directory*/

	bool tryagain = false; /*will be used to indicate a particular error with stat*/

	errno = 0; /*to check after calling stat*/

	/*use stat instead of lstat if symbolic*/
	if(symbolic){
		
		/*error checking*/
		if(stat(directory, &filestat)==-1){
			/*symbolic link loop gets special error message*/
			if(errno == 40){ /*ELOOP when calling stat*/
				fprintf(stderr, "fiend: Loop at %s\n", directory);
				return 1;
			}

			/*if the error was about something being wrong with the linked file, we should try again to process using lstat (i.e. ignore the link)*/
			if(errno == 14){
				tryagain = true;
			}

			/*any other error gets generic error message*/
			else if(errno != 14){
				fprintf(stderr, "fiend: Invalid filename: %s\n", directory);
				return 1;
			}
		}

		/*check if directory, set flag to search it if so*/
		else{
			if(S_ISDIR(filestat.st_mode)){
				go_further = true;
			}
		}
	}

	errno = 0; /*reset errno just in case*/

	/*if not symbolic use lstat (or if error reading linked file described above)*/
	if(!symbolic || tryagain){
		
		/*if error calling stat return error and exit*/
		if(lstat(directory, &filestat) == -1){
			fprintf(stderr, "fiend: Invalid filename: %s\n", directory);
			return 1;
		}

		/*check if directory, set flag to search it if so*/
		else{
			if(S_ISDIR(filestat.st_mode)){
				go_further = true;
			}
		}
	}

	int cur_inode = filestat.st_ino; /*inode number of current file*/
	
	Stack search = cur_parent; /*begin inode search at the current parent*/

	/*continue searching until at the end of the list*/
	while(search !=0){

		/*if cur_inode matches a previous inode, we found a loop*/
		if(search->inode == cur_inode){
			fprintf(stderr, "fiend: Loop at %s\n", directory);
				return 1;
		}

		/*next in list*/
		else{
			search = search->parent;
		}
	}

	/*if not depth, process file before searching directory*/
	if(!dfs){
		processfile(directory, expressions, exprowlengths, expnumrows, symbolic);
	}

	/*if the current file is a directory and we haven't hit the maxdepth*/
	if(go_further && (maxdepth ==-1 || level+1 <= maxdepth)){

		errno = 0; /*to error check opening the directory*/

		dir = opendir(directory); /*open the directory*/

		struct dirent *entry; /*an entry in the directory*/

		/*if error opening directory, exit*/
		if(dir == NULL || errno != 0){
			fprintf(stderr, "fiend: Error opening directory %s\n", directory);
			return(1);
		}

		/*otherwise, search the directory*/
		else{

			stackPush(&cur_parent, cur_inode); /*add the current inode to the head of the linked list*/

			/*loop through directory entries*/
			while((entry = readdir(dir)) != NULL){			

				/*only read entries that are actually children of the current file*/
				if(!(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0)){

					/*append the filename to the current long filename for printing purposes*/
					/*alloc space for new string*/
					char* full_filename = malloc((strlen(directory)+strlen(entry->d_name)+2)*sizeof(char));

					strcpy(full_filename, directory);

					/*ensure "/" is between files*/
					if(full_filename[strlen(directory)-1] != '/'){
						strcat(full_filename,  "/");
					}

					strcat(full_filename, entry->d_name);

					/*send the entry to recursion*/
					recursion(full_filename, level+1, maxdepth, expressions, exprowlengths, expnumrows, dfs, symbolic, cur_parent);

					/*free malloced memory now that we're done with it*/
					free(full_filename);
				}
			}

			free(cur_parent); /*free malloced memory now that we're done with it*/
		}

		closedir(dir); /*close the directory*/
	}

	/*if -depth, then process file after processing children*/
	if(dfs){
		processfile(directory, expressions, exprowlengths, expnumrows, symbolic);
	}

	return 0;
}


/*ATTRIBUTION: linked list code from "Notes on Data Structures and Programming Techniques (CPSC 223, Spring 2018)" by James Aspnes*/
void stackPush(Stack *s, int value){
	Stack e;
	e = malloc(sizeof(struct inodes_list));
	/*assert(e);*/
	e->inode = value;
	e->parent = *s;
	*s = e;
}

int stackEmpty(const Stack *s){
	return (*s == 0);
}
/*END ATTRIBUTION*/


int processfile(const char* file, struct exp** expressions, int* exprowlengths, int expnumrows, bool symbolic){

	char* cur_file = strdup(file); /*copy the filename to make sure we don't change it*/

	bool logic; /*keep track of whether expressions are currently evaluating to true or false*/

	struct exp cur_exp; /*will be used to point to the current expression as we loop through the exps*/

	struct stat status; /*for use with stat*/

	struct stat comp_status; /*for use with stat an -newer*/

	/*use the appropriate stat or lstat. we may assume stat does not fail since this file was successfully stat-ed in recursion*/
	if(symbolic){
		stat(cur_file, &status);
	}

	else{
		lstat(cur_file, &status);
	}

	/*loop through the rows of exp array*/
	for(int i = 0; i < expnumrows; i++){
			
		logic = false; /*logic is false until a exp returns true*/

		/*loop through the exps in each row*/
		for(int j = 0; j < exprowlengths[i]; j++){

			cur_exp = expressions[i][j]; /*to shorten notation*/

			/*check the ID of the exp*/
			if(cur_exp.id == name){

				char*file_suffix = strdup(cur_file); /*make a copy of cur_file so that basename doesn't change cur_file*/

				/*use fnmatch with flags set to 0 to do regex checking for -name*/
				/*use basename to get just the last part of the name of the file*/
				if(fnmatch(cur_exp.data, basename(file_suffix), 0) == 0){
					logic = true; /*if it matches, logic is true*/
				}
				else{
					logic = false; /*if it doesn't match, logic is false*/
				}

				free(file_suffix);
			}

			else if(cur_exp.id == newer){
				
				/*depending on symbolic, use lstat or stat to get data about the reference file*/
				if(symbolic){
					stat(cur_exp.data, &comp_status);
				}
				else{
					lstat(cur_exp.data, &comp_status);
				}

				/*compare times down to the nanosecond*/
				if(status.st_mtim.tv_nsec > comp_status.st_mtim.tv_nsec){
					logic = true; /*true if file is newer than reference*/
				}
				else{
					logic = false;
				}

			}

			else if(cur_exp.id == print){

				printf("%s\n", cur_file);

				logic = true; /*-print always returns true*/
			}

			else if(cur_exp.id == exec){

				int alloc_command = strlen(cur_exp.data) + 1; /*alloced length of command*/

				int length_command = 0; /*actual length of command*/

				char *command = malloc(alloc_command*sizeof(char));

				int recent_paren = 0; /*keep track of the index of the most recent parenthesis*/
					
				/*loop through the command given to -exec to look for '{}'*/
				for(int k = 0; k < strlen(cur_exp.data)-1; k++){

					/*if found a '{}'*/
					if((cur_exp.data[k] == '{') && (cur_exp.data[k+1] == '}')){

						/*ensure alloced space is enough to add the filename*/
						while((length_command + strlen(cur_file) + k - recent_paren + 1) >= alloc_command){
							alloc_command = 2*alloc_command;
							command = realloc(command, alloc_command*sizeof(char));
						}

						/*if command stile empty we need to use strcpy instead of strcat*/
						if(length_command == 0){

							/*if '{}' begins the command we'll just copy the filename to command*/
							if(k - recent_paren == 0){
								strcpy(command, cur_file);
							}

							/*if not, we need to copy each character prior to '{}' to command*/
							else{

								int l;

								for(l=recent_paren; l < k; l++){
									command[l] = cur_exp.data[l];
								}

								command[l] = '\0'; /*add a null terminator so that we can use strcat*/
																
								strcat(command, cur_file); /*add the filename in place of {}*/
							}	
						}

						/*if command is not empty we can use strncat and strcat*/
						else{
							strncat(command, cur_exp.data + recent_paren, k-recent_paren); /*add (k - recent paren) chars to command beginning after the most recent '{}' */

							strcat(command, cur_file); /*add the filename in place of {}*/
						}

						length_command += strlen(cur_file) + k - recent_paren; /*update length*/

						recent_paren = k+2; /*update index of recent paren (this is actually the index immediately following the closing paren*/
					}
				}

				/*if no '{}' was found*/
				if(recent_paren == 0){

					strcpy(command, cur_exp.data); /*just copy the given command to command*/
				}

				/*if at least one '{}' was found, we need to add the end of the given command to command*/
				else{
					/*make sure there's enough space*/
					while((length_command + strlen(cur_exp.data) - recent_paren) >= alloc_command){
						alloc_command = 2*alloc_command;
						command = realloc(command, alloc_command*sizeof(char));
					}

					strncat(command, cur_exp.data + recent_paren, strlen(cur_exp.data)-recent_paren);
				}	

				fflush(stdout); /*necessary to call before the call to system*/

				if(system(command) == 0){
					/*system returns 0 when true*/
					logic = true;
				}

				else{
					logic = false;
				}

				free(command);
			}

			/*exps in a row are separated by -a, so if the logic is currently false then the whole row is false so we move to the next one*/
			if(!logic){
				break;
			}
		}

		/*rows of exps are separated by -o, so if a whole row evaluates to true, we are done*/
		if(logic){
			break;
		}
	}

	free(cur_file);

	return 0;
}
