//Chris West cgw29

#define _GNU_SOURCE
#include "/c/cs323/Hwk2/parsley.h"
#include "/c/cs323/Hwk5/process-stub.h"


//keep track of the PIDs of backgrounded processes
typedef struct pidlist{
	pid_t* list; //pid list
	int* completed; //list of 0s and 1s to indicate if a PID has been waited for
	int num_entries; //number of entries in the list
    int num_alloc; //malloced space for the list
} PIDLIST;

//flatten a list of sequences in order to implement &
typedef struct seqlist{
	const CMD** list; //list of commands
	int num_entries; //number of commands
    int num_alloc; //malloced length of list
} SEQLIST;


//primary functions that match the structure of a command tree
int handle_simple(CMD command, PIDLIST* mypids);
int handle_pipe(const CMD* tree, PIDLIST* mypids);
int handle_andor(const CMD* tree, PIDLIST* mypids);
int handle_seq(const CMD* tree, PIDLIST* mypids, int sub);

//flattens the sequences in a command tree and stores commands in myseqs
void flatten_seq(const CMD* tree, SEQLIST* myseqs);

//helper functions for seqlist and pidlist
void add_seq(SEQLIST* myseqs, const CMD* new_cmd);
void add_pid(PIDLIST* mypids, pid_t new_pid);

//set and restore redirects
int handle_redirects(CMD command);
void restore_redirects(int fd_in, int fd_out, int fd_err);

//identify and call built-in functions
int handle_builtin(const CMD* tree, PIDLIST* mypids, int sub);

//built-in functions
int cd(char**argv);
int export(char**argv);
int my_wait(char**argv, PIDLIST* mypids);

//from spec
#define STATUS(x) (WIFEXITED(x) ? WEXITSTATUS(x) : 128+WTERMSIG(x))

//takes a CMD* cmdlist and executes the commands in the tree
int process(const CMD* cmdlist){

	//mypids is a static variable, only initialize if it has not yet been
	static PIDLIST* mypids;
	if(mypids == NULL){
		mypids = malloc(sizeof(PIDLIST));
		mypids->num_alloc = 2;
		mypids->completed = malloc(mypids->num_alloc*sizeof(int));
		mypids->list = malloc(mypids->num_alloc*sizeof(pid_t));
		mypids->num_entries = 0;
	}

	//store the return of handle_seq
	//0 indicates we're not coming from a subcommand
	int status = handle_seq(cmdlist, mypids, 0);

	//convert status to string to set environment variable ?
	int length = snprintf( NULL, 0, "%d", status);
	char* status_str = malloc((length + 1)*sizeof(char));
	snprintf(status_str, length + 1, "%d", status);

	setenv("?", status_str, 1);
	//don't check for errors here to avoid recursion

	free(status_str);

	//status for checking for zombies
	int zombie_status = 0;

	//loop through pidlist
	for(int i = 0; i < mypids->num_entries; i++){

		//check if zombie
		if(mypids->list[i] == waitpid(mypids->list[i], &zombie_status, WNOHANG)){
			//kill the zombie
			waitpid(mypids->list[i], &zombie_status, 0);
			fprintf(stderr, "Completed: %d (%d)\n", mypids->list[i], STATUS(zombie_status));
			//record that the zombie has been completed
			mypids->completed[i] = 1;
		}
	}

	return status;
}

//handle commands of type simple
int handle_simple(CMD command, PIDLIST* mypids){
		
	//pid for forking
	pid_t pid;

	//status for waitpid
	int status = 0;

	//to record errno
	int myerrno;

	//to record the return value of functions
	int retval = 0;

	//use a subshell to execute the simple
	if((pid = fork()) < 0){
		//IF ERROR

		myerrno = errno;
		perror("bashLT");
		return myerrno;
	}
	else if(pid == 0){
		//IF CHILD

		//handle redirects, exit if error
		if((retval = handle_redirects(command)) != 0){
			exit(retval);
		}

		//set local variables, exit if error
		for(int i = 0; i < command.nLocal; i++){
			retval = setenv(command.locVar[i], command.locVal[i], 1); //1 => overwrite var if already present
			
			if(retval == -1){
				myerrno = errno;
				perror("bashLT");
				exit(myerrno);
			}
		}

		//if it's a subcommand, call handle_seq and indicate we're in a subcmd
		if(command.type == SUBCMD){
			exit(handle_seq(command.left, mypids, 1));
		}

		//check for built-ins
		else if(strcmp(command.argv[0], "cd")==0){
			exit(cd(command.argv));
		}
		else if(strcmp(command.argv[0], "export")==0){
			exit(export(command.argv));
		}
		//wait has no effect if in a pipe or andor
		else if(strcmp(command.argv[0], "wait")==0){
			exit(0);
		}
		//otherwise, use execvp to execute command
		else{
			retval = execvp(command.argv[0], command.argv);

			//exit if error
			if(retval == -1){
				myerrno = errno;
				perror("bashLT");
				exit(myerrno);
			}

			//exit child
			exit(retval);
		}
	}
	else{
		//IF PARENT

		//ignore sigints
		signal(SIGINT, SIG_IGN);

		//wait for child
		waitpid(pid, &status, 0); //flag = 0 means no options

		//restore sigint
		signal(SIGINT, SIG_DFL);

		//return
		return STATUS(status);
	}
}

//handle commands of type pipe
int handle_pipe(const CMD* tree, PIDLIST* mypids){

	//PIDs of left and right children
	pid_t pid_left;
	pid_t pid_right;

	//status vars for left and right children
	int status_left = 0;
	int status_right = 0;

	//to record errno
	int myerrno;

	//if type is not a pipe, pass it along to handle_simple
	if(tree->type != PIPE){
		return handle_simple(*tree, mypids);
	}

	//else it's a pipe
	else{

		//to get pipe fds
		int pipefds[2];

		//get pipe fds and check for error
		if(pipe(pipefds) == -1){
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		//if no error, fork for left child
		if((pid_left = fork()) < 0){
			//IF ERROR
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		else if(pid_left == 0){
			//IF (LEFT) CHILD

			//set fds
			dup2(pipefds[1], 1);
			close(pipefds[1]);

			//recursion down the left branch of the tree
			exit(handle_pipe(tree->left, mypids));
		}

		else{
			//IF PARENT

			//close pipefds[1] here too to stay aligned
			close(pipefds[1]);

			//fork for right child
			if((pid_right = fork()) < 0){
				//IF ERROR

				myerrno = errno;
				perror("bashLT");
				return myerrno;
			}

			else if(pid_right == 0){
				//IF (RIGHT) CHILD

				//set fds
				dup2(pipefds[0], 0);
				close(pipefds[0]);

				//right child of a pipe is a simple
				exit(handle_simple(*tree->right, mypids));
			}
			else{
				//IF PARENT

				//ignore sigints
				signal(SIGINT, SIG_IGN);

				//wait here for both left and right to allow parallel execution
				waitpid(pid_left, &status_left, 0);

				waitpid(pid_right, &status_right, 0);

				//restore sigint
				signal(SIGINT, SIG_DFL);
			}

			//status is of rightmost failing pipe, 0 -> didn't fail
			if(status_right == 0){
				return STATUS(status_left);
			}
			else{
				return STATUS(status_right);
			}
		}
	}
}

//handle commands of type andor
int handle_andor(const CMD* tree, PIDLIST* mypids){

	//PIDs of left and right children
	pid_t pid_left;
	pid_t pid_right;

	//status vars for left and right children
	int status_left = 0;
	int status_right = 0;

	//to record errno
	int myerrno;

	//if not an andor, call handle_pipe
	if(tree->type != SEP_AND && tree->type != SEP_OR){
		return handle_pipe(tree, mypids);
	}

	//else it's a andor
	else{

		//fork for left child
		if((pid_left = fork()) < 0){
			//IF ERROR

			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}
		else if(pid_left == 0){
			//IF (LEFT) CHILD

			//recursion
			exit(handle_andor(tree->left, mypids));
		}
		else{
			//IF PARENT

			//ignore sigints
			signal(SIGINT, SIG_IGN);

			//wait for left to finish
			waitpid(pid_left, &status_left, 0);

			//restore sigint
			signal(SIGINT, SIG_DFL);

			//determine whether to exit now
			if(tree->type == SEP_AND && STATUS(status_left) != 0){
				return STATUS(status_left);
			}
			else if(tree->type == SEP_OR && STATUS(status_left) == 0){
				return STATUS(status_left);
			}

			//if not exited and right branch is not null
			if(tree->right != NULL){

				//fork for right child
				if((pid_right = fork()) < 0){
					//IF ERROR

					myerrno = errno;
					perror("bashLT");
					return myerrno;
				}
				else if(pid_right == 0){
					//IF (RIGHT) CHILD

					//recursion
					exit(handle_andor(tree->right, mypids));
				}
				else{
					//IF PARENT

					//ignore sigints
					signal(SIGINT, SIG_IGN);

					//wait for right child
					waitpid(pid_right, &status_right, 0);

					//restore sigint
					signal(SIGINT, SIG_DFL);

					//return the status of right child
					return STATUS(status_right);
				}				
			}

			//if no right child, return the status of left child
			return STATUS(status_left);
		}
	}
}

//handle commands of type sequence
int handle_seq(const CMD* tree, PIDLIST* mypids, int sub){
	//sub == 1 if we're in a subcommand

	//if not a sequence, call handle_builtin
	if(tree->type != SEP_BG && tree->type != SEP_END){
		return handle_builtin(tree, mypids, sub);
	}

	//malloc array for a list of commands of type seq
	SEQLIST* myseqs = malloc(sizeof(SEQLIST));
	myseqs->num_entries = 0;
	myseqs->num_alloc = 2;
	myseqs->list = malloc(myseqs->num_alloc*sizeof(CMD*));

	//flatten the sequences
	flatten_seq(tree, myseqs);

	//return variable
	int ret;

	//pid for forking
	pid_t pid;

	//to record errno
	int myerrno;

	//loop through sequences in seqlist
	for(int i = 0; i < myseqs->num_entries; i++){

		//if at the end of the list
		if(i == myseqs->num_entries - 1){

			//pass command through
			ret = handle_builtin(myseqs->list[i], mypids, sub);

			//free the seqlist
			free(myseqs->list);
			free(myseqs);

			//and return
			return ret;
		}

		//if command is followed by a ;
		else if(myseqs->list[i+1]->type == SEP_END){

			//pass command through
			ret = handle_builtin(myseqs->list[i], mypids, sub);
		}

		//if command is followed by a &, we need to background it
		else if(myseqs->list[i+1]->type == SEP_BG){
				
			//fork
			if((pid = fork()) < 0){
				//IF ERROR

				myerrno = errno;
				perror("bashLT");

				//free seqlist
				free(myseqs->list);
				free(myseqs);

				return myerrno;
			}

			else if(pid == 0){
				//IF CHILD

				//pass the command through and exit
				exit(handle_andor(myseqs->list[i], mypids));
			}

			else{	
				//IF PARENT

				//print line to stderr
				fprintf(stderr, "Backgrounded: %d\n", pid);

				//record PID of backgrounded command
				add_pid(mypids, pid);

				//status of backgrounded command is 0
				ret = 0;
			}
		}

		//skip the sequence separator
		i++;

		//if a separator ends the list
		if(i == myseqs->num_entries - 1){

			//free seqlist
			free(myseqs->list);
			free(myseqs);

			//and return
			return ret;
		}
	}

	//when done, free seqlist
	free(myseqs->list);
	free(myseqs);

	//and return (the status of the leftmost command)
	return ret;

}
	
//take a cmd tree and flatten the subsequent commands of type seq
void flatten_seq(const CMD* tree, SEQLIST* myseqs){

	//if at the end of the sequences
	if(tree->left->type != SEP_BG && tree->left->type != SEP_END){
		
		//leftmost command goes first in list
		add_seq(myseqs, tree->left);

		//record the parent too to keep track of the separator type (; or &)
		add_seq(myseqs, tree);

		//then add the right branch if not null
		if(tree->right != NULL){
			add_seq(myseqs, tree->right);
		}
	}

	//if not at the end of the sequences
	else{
		//recursion down left branch
		flatten_seq(tree->left, myseqs);

		//then add the parent too
		add_seq(myseqs, tree);

		//then add the right branch if not null
		if(tree->right != NULL){
			add_seq(myseqs, tree->right);
		}
	}
}

//adds a command to a seqlist
void add_seq(SEQLIST* myseqs, const CMD* new_cmd){

	//realloc if necessary
	if(myseqs->num_entries + 1 > myseqs->num_alloc){
		myseqs->num_alloc *= 2;
		myseqs->list = realloc(myseqs->list, myseqs->num_alloc*sizeof(CMD*));
	}

	//and the command
	myseqs->list[myseqs->num_entries] = new_cmd;

	//update number of entries
	myseqs->num_entries++;
}

//adds a PID to a pidlist
void add_pid(PIDLIST* mypids, pid_t new_pid){

	//realloc if necessary
	if(mypids->num_entries + 1 > mypids->num_alloc){
		mypids->num_alloc *= 2;
		mypids->list = realloc(mypids->list, mypids->num_alloc*sizeof(pid_t));
		mypids->completed = realloc(mypids->completed, mypids->num_alloc*sizeof(int));
	}

	//add the pid
	mypids->list[mypids->num_entries] = new_pid;
	
	//initialize completed to 0
	mypids->completed[mypids->num_entries] = 0;
	
	//update number of entries	
	mypids->num_entries++;
}

//handle the redirects for a command
int handle_redirects(CMD command){

	//file descriptor
	int fds;

	//to record errno
	int myerrno;

	//check all .fromTypes and .toTypes
	if(command.fromType == RED_IN){

		//open as read only
		fds = open(command.fromFile, O_RDONLY);

		//check for error
		if(fds == -1){
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		//dup2 and close
		dup2(fds, 0);
		close(fds);
	}

	if(command.fromType == RED_IN_HERE){
		//step 1 make a temp file
		char* static_template = "/tmp/bashLT_HERE_DOC_XXXXXX";
		char* template = strdup(static_template);
		fds = mkstemp(template);

		//check for mkstemp error
		if(fds == -1){
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		//step 2, write contents to file
		size_t nbyte = sizeof(char)*strlen(command.fromFile);
		write(fds, command.fromFile, nbyte);
		
		//step 3, close the fd
		close(fds);

		//open the temp file as readonly
		fds = open(template, O_RDONLY);

		//check for error
		if(fds == -1){
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		//dup2 and close
		dup2(fds, 0);
		close(fds);
	}

	if(command.toType == RED_OUT){

		//open as write only, create if nonexistant, and delete current contents
		fds = open(command.toFile, O_WRONLY | O_CREAT | O_TRUNC, 00664);

		//check for error
		if(fds == -1){
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		//dup2 and close
		dup2(fds, 1);
		close(fds);
	}

	if(command.toType == RED_OUT_ERR){

		//open as write only, create if nonexistant, and delete current contents
		fds = open(command.toFile, O_WRONLY | O_CREAT | O_TRUNC, 00664);

		//check for error
		if(fds == -1){
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		//dup2 (into fd 2 = stderr) and close
		dup2(fds, 2);
		close(fds);
	}

	if(command.toType == RED_OUT_APP){

		//open as write only, create if nonexistant, and append to current contents
		fds = open(command.toFile, O_WRONLY | O_APPEND | O_CREAT, 00664);
		
		//check for error
		if(fds == -1){
			myerrno = errno;
			perror("bashLT");
			return myerrno;
		}

		//dup2 and close
		dup2(fds, 1);
		close(fds);
	}

	//nothing failed, return 0
	return 0;
}
	
//given old fds, restores them
void restore_redirects(int fd_in, int fd_out, int fd_err){
	dup2(fd_in, 0);
	dup2(fd_out, 1);
	dup2(fd_err, 2);
	close(fd_in);
	close(fd_out);
	close(fd_err);
}

//checks for and handles builtin functions
int handle_builtin(const CMD* tree, PIDLIST* mypids, int sub){

	//keep track of return values
	int ret;

	//to record old fds
	int fd_in;
	int fd_out;
	int fd_err;

	//only can be a builtin if type is simple
	if(tree->type == SIMPLE){

		//if cd
		if(strcmp(tree->argv[0], "cd") == 0){

			//record old fds
			fd_in = dup(0);
			fd_out = dup(1);
			fd_err = dup(2);

			//handle redirects and check for error
			if((ret = handle_redirects(*tree)) != 0){
				return ret;
			}

			//execute cd
			ret = cd(tree->argv);

			//restore the redirects
			restore_redirects(fd_in, fd_out, fd_err);

			//and return
			return ret;
		}

		//if export
		else if(strcmp(tree->argv[0], "export") == 0){

			//record old fds
			fd_in = dup(0);
			fd_out = dup(1);
			fd_err = dup(2);

			//handle redirects and check for error
			if((ret = handle_redirects(*tree)) != 0){
				return ret;
			}

			//execute export
			ret = export(tree->argv);

			//restore the redirects
			restore_redirects(fd_in, fd_out, fd_err);

			//and return
			return ret;
		}

		//if wait
		else if(strcmp(tree->argv[0], "wait") == 0){

			//wait does nothing if we're in a subcommand
			if(sub == 1){
				return 0;
			}

			//record old fds
			fd_in = dup(0);
			fd_out = dup(1);
			fd_err = dup(2);

			//handle redirects and check for error
			if((ret = handle_redirects(*tree)) != 0){
				return ret;
			}

			//execute wait
			ret = my_wait(tree->argv, mypids);

			//restore the redirects
			restore_redirects(fd_in, fd_out, fd_err);

			//and return
			return ret;
		}

		//otherwise its not a builtin
		else{
			return handle_andor(tree, mypids);
		}
	}

	//if not a simple, pass through to handle_andor
	else{
		return handle_andor(tree, mypids);
	}

}

//builtin: cd
int cd(char**argv){

	//record errno
	int myerrno;

	//if just "cd", cd to home
	if(argv[1] == NULL){

		//to save the pathname
		char* path;

		//find the pathname for "HOME", error if NULL
		if((path = getenv("HOME")) == NULL){
			myerrno = errno;
			perror("cd");
			return myerrno;
		}

		//change directory to home, check for error
		if(chdir(path) == -1){
			myerrno = errno;
			perror("cd");
			return myerrno;
		}

		//nothing failed
		return 0;
	}
	//if "cd -p" print pwd
	else if(strcmp(argv[1], "-p") == 0){

		//if another argv follows, error
		if(argv[2] != NULL){
			fprintf(stderr, "cd: usage\n");
			return 1;
		}

		//malloc size is 2^power
		int power=7;

		//malloc a buffer
		char* buf = malloc((1<<power)*sizeof(char));

		//getcwd and store it in buffer, resizing if too small
		while(getcwd(buf, (1<<power)*sizeof(char))==NULL){

			//ERANGE means buffer was too small, so realloc
			if(errno == ERANGE){
				power++;
				buf = realloc(buf, (1<<power)*sizeof(char));
			}

			//otherwise it was a real error
			else{
				//save errno
				myerrno = errno;
				perror("cd");

				//free the buffer
				free(buf);

				//and return
				return myerrno;
			}
		}

		//use dprintf to ensure print to current fd 1
		dprintf(1, "%s\n", buf);

		//free the buffer
		free(buf);

		//nothing failed
		return 0;
	}

	//else "cd [DIRECTORY]"
	else{

		//check proper usage
		if(argv[2] != NULL){
			fprintf(stderr, "usage: cd [DIRECTORY]\n");
			return 1;
		}

		//change to given directory, check for error
		if(chdir(argv[1])== -1){
			myerrno = errno;
			perror("cd");
			return myerrno;
		}

		//nothing failed
		return 0;
	}
}

//builtin: export
int export(char**argv){

	//return value
	int ret;

	//to record errno
	int myerrno;

	//check proper usage
	if(argv[1] == NULL){
		fprintf(stderr, "usage: export N=V or export -n N\n");
		return 1;
	}

	//if "export -n N"
	if(strcmp(argv[1], "-n")==0){

		//make sure a variable is given
		if(argv[2] == NULL){
			fprintf(stderr, "usage: export N=V or export -n N\n");
			return 1;
		}
			
		//unset given var and check for error
		if(unsetenv(argv[2]) == -1){
			myerrno = errno;
			perror("export");
			return myerrno;
		}
			
		//nothing failed
		return 0;
	}

	//if "export N=V"
	else{

		//check for proper usage
		if(argv[2] != NULL){
			fprintf(stderr, "usage: export N=V or export -n N\n");
			return 1;
		}

		//check that "N=V" is in valid form with valid chars
		if(strchr(argv[1], '=') == NULL || strlen(argv[1]) < 2 || strchr("0123456789", argv[1][0]) != NULL){
			fprintf(stderr, "usage: export N=V or export -n N\n");
			return 1;
		}

		//two strings, one for variable name and one for value
		char* varstr = malloc(strlen(argv[1])*sizeof(char));
		char* valstr = malloc(strlen(argv[1])*sizeof(char));

		//to write into new strings
		int varindex = 0;
		int valindex = 0;

		int index = 0;

		//go through argv[1] until we get to the equals sign
		while(argv[1][index] != '='){

			//check that its a valid variable character
			if(strchr(VARCHR, argv[1][index]) == NULL){
				//if not, return
				free(varstr);
				free(valstr);
				fprintf(stderr, "usage: export N=V or export -n N\n");
				return 1;
			}

			//copy into variable string
			varstr[varindex] = argv[1][index];

			//increment indices
			varindex++;
			index++;
		}

		//null terminate variable string
		varstr[varindex] = '\0';
		index++;

		//read the rest of the string into value
		for(;index < strlen(argv[1]); index++){
			valstr[valindex] = argv[1][index];
			valindex++;
		}

		//and null terminate it
		valstr[valindex] = '\0';

		//set environmentvariable
		ret = setenv(varstr, valstr, 1);
		
		free(varstr);
		free(valstr);

		//check for error
		if(ret == -1){
			myerrno = errno;
			perror("export");
			return myerrno;
		}

		//nothing failed
		return 0;
	}
}

//builtin: wait
int my_wait(char**argv, PIDLIST* mypids){

	//check for proper usage
	if(argv[1] != NULL){
		fprintf(stderr, "usage: wait\n");
		return 1;
	}

	//to record status
	int status = 0;

	pid_t pid;

	//to record errno
	int myerrno;

	//loop through all PIDs in list
	for(int i = 0; i < mypids->num_entries; i++){

		//if current PID has not been waited for or reaped yet
		if(mypids->completed[i] == 0){

			//use a subshell so that sigints are handled correctly
			if((pid = fork()) < 0){
				//IF ERROR

				myerrno = errno;
				perror("bashLT");
				return myerrno;
			}
			else if(pid == 0){
				//IF CHILD

				//wait for it and record its status
				waitpid(mypids->list[i], &status, 0);

				exit(status);
			}
			else if(pid > 0){
				//IF PARENT

				//ignore sigints
				signal(SIGINT, SIG_IGN);

				//wait for child
				waitpid(pid, &status, 0);

				//wait again for pid to make it not a zombie
				waitpid(mypids->list[i], &status, 0);

				//restore sigint
				signal(SIGINT, SIG_DFL);

				//print summary to stderr
				fprintf(stderr, "Completed: %d (%d)\n", mypids->list[i], STATUS(status));
				
				//mark PID as completed
				mypids->completed[i] = 1;
			}
		}		
	}

	//return
	return 0;
}