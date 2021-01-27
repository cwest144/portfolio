//Chris West, cgw29

#define _GNU_SOURCE
#include "/c/cs323/Hwk2/parsley.h"

//struct that lexer will record tokens with
typedef struct token{
	int type;
	char** argv;
	int argc;
	int beg;
} TOKEN;

//the list of tokens
typedef struct token_arr{
	TOKEN* list;
	int length;
	int alloc;
} TOKEN_ARR;

//the following three structs are used to record information when parsing and then the info is passed to the created node
typedef struct local_var{
	char** Vars;
	char** Vals;
	int num;
	int alloc;
} LOCAL_VAR;

typedef struct redirect_var{
	int fromtype;
	char* fromfile;

	int totype;
	char* tofile;
} REDIRECT_VAR;

typedef struct argv_var{
	int argc;
	char** argv;
	int alloc;
} ARGV_VAR;

/*ATTRIBUTION: linked list code from "Notes on Data Structures and Programming Techniques (CPSC 223, Spring 2018)" by James Aspnes*/
struct element {
	int val;
	struct element *parent;
};

typedef struct element *STACK;

#define STACK_EMPTY (0)

int stackEmpty(const STACK*s);

void stackPush(STACK *s, int value);

int stackPop(STACK *s);
/*END ATTRIBUTION*/

//lexing function
//takes a line and a pointer to an array of tokens
//lexes the line and records info as tokens in the array pointed to
int lexer(char* line, TOKEN_ARR* my_list);

//parsing function
//takes a pointer to an array of tokens
//returns a pointer to the head of a command tree that represents the array of tokens
CMD *parser(TOKEN_ARR*my_list);

//parse recursive functions
int local(TOKEN_ARR*my_list, int*position, LOCAL_VAR*my_locals);
int redirect(TOKEN_ARR*my_list, int*position, REDIRECT_VAR*my_redirects, bool*error_flag, bool*from_pipe);
int prefix(TOKEN_ARR*my_list, int*position, LOCAL_VAR*my_locals, REDIRECT_VAR*my_redirects, bool*error_flag, bool*from_pipe);
int suffix(TOKEN_ARR*my_list, int*position, REDIRECT_VAR*my_redirects, ARGV_VAR*my_argvs, bool*error_flag, bool*from_pipe);
CMD* simple(TOKEN_ARR*my_list, int*position, bool*error_flag, bool*from_pipe);
CMD* subcmd(TOKEN_ARR*my_list, int*position, bool*error_flag, bool*from_pipe);
CMD* stage(TOKEN_ARR*my_list, int*position, bool*error_flag, bool*from_pipe);
CMD* pipeline(TOKEN_ARR*my_list, int*position, bool*error_flag);
CMD* andor(TOKEN_ARR*my_list, int*position, bool*error_flag);
CMD* sequence(TOKEN_ARR*my_list, int*position, bool*error_flag);
CMD* command(TOKEN_ARR*my_list, int*position, bool*error_flag);

//initialize and free functions for often used structs
void inittoken(TOKEN* list, int index, int type);

void initlocals(LOCAL_VAR*my_locals);
void freelocals(LOCAL_VAR*my_locals);

void initredirects(REDIRECT_VAR*my_redirects);
void freeredirects(REDIRECT_VAR*my_redirects);

void initargvs(ARGV_VAR*my_argvs);
void freeargvs(ARGV_VAR*my_argvs);

//check if the current type indicates the end of a simple
#define CMD_END(type) (type == PIPE  || type == SEP_AND || \
		      type == SEP_OR || type == SEP_BG || \
		      type == SEP_END || type == PAR_RIGHT || \
		      type == NONE)

//Parse function
CMD *parse(char* line){

	//allocate struct to hold pointer to token array
	TOKEN_ARR* my_list = malloc(sizeof(TOKEN_ARR));
	my_list->length = 0;
	my_list->alloc = 4;
	my_list->list = malloc(my_list->alloc*sizeof(TOKEN));

	//lex the line
	lexer(line, my_list);

	CMD*my_tree; //command tree head

	//create a stack to check if the line has balanced paranthesis
	STACK my_stack;
	my_stack = STACK_EMPTY;
	int index = 0;
	bool balanced = true;

	//loop through the list of tokens
	while(my_list->list[index].type !=NONE){
		//push an arbitrary value when a left parenthesis is read
		if(my_list->list[index].type == PAR_LEFT){
			stackPush(&my_stack, 1);
		}

		//pop value when a right paranthesis is read
		else if(my_list->list[index].type == PAR_RIGHT){
			//if the stack is empty then error
			if(stackEmpty(&my_stack)){
				fprintf(stderr, "parsley: unbalanced paranthesis\n");
				balanced = false;
				break;
			}
			else{
				stackPop(&my_stack);
			}
		}

		index++;
	}

	//if we read through the line and stack is not empty then there are too many left parens
	if(!stackEmpty(&my_stack)){
		fprintf(stderr, "parsley: unbalanced paranthesis\n");
		balanced = false;
		while(!stackEmpty(&my_stack)){
			stackPop(&my_stack);
		}
	}

	//if there are no tokens or the tree is unbalances set my_tree to null and don't parse anything
	if(my_list->length == 0 || !balanced){
		my_tree = NULL;
	}

	//else, parse the list of tokens
	else{
		my_tree = parser(my_list);
	}

	//free the list of tokens
	index = 0;
	while(my_list->list[index].type != NONE){
		if(my_list->list[index].type == TEXT){
			
			for(int i = 0; i < my_list->list[index].argc; i++){
				free(my_list->list[index].argv[i]);
			}
			free(my_list->list[index].argv);
		}
		index++;
	}
	free(my_list->list);
	free(my_list);

	//return the head command
	return my_tree;
}


//Lexer function
int lexer(char* line, TOKEN_ARR* my_list){

	int line_length = strlen(line);

	char cur_char;

	bool comment = false; //true if a comment character has been read

	//loop through the line reading one character at a time
	for(int i = 0; i < line_length; i++){

		//skip spaces
		while(isspace(line[i]) && i+1 < line_length){
			i++;
		}

		//if the line ends with a space then we're done reading
		if(isspace(line[i]) && i+1 == line_length){
			break;
		}

		cur_char = line[i]; //the current character

		//figure out which token the current character is
		//reading the next char if necessary
		if(cur_char == '|'){

			if(i+1 < line_length && line[i+1] == '|'){
				inittoken(my_list->list, my_list->length, SEP_OR);
				my_list->length++;
				i++;
			}

			else{
				inittoken(my_list->list, my_list->length, PIPE);
				my_list->length++;
			}
		}

		else if(cur_char == '&'){
			if(i+1 < line_length && line[i+1] == '&'){
				inittoken(my_list->list, my_list->length, SEP_AND);
				my_list->length++;
				i++;
			}

			else if (i+1 < line_length && line[i+1] == '>'){
				inittoken(my_list->list, my_list->length, RED_OUT_ERR);
				my_list->length++;
				i++;
			}

			else{
				inittoken(my_list->list, my_list->length, SEP_BG);
				my_list->length++;
			}
		}
		else if(cur_char == ';'){
			inittoken(my_list->list, my_list->length, SEP_END);
			my_list->length++;
		}

		else if(cur_char == '('){
			inittoken(my_list->list, my_list->length, PAR_LEFT);
			my_list->length++;
		}

		else if(cur_char == ')'){
			inittoken(my_list->list, my_list->length, PAR_RIGHT);
			my_list->length++;
		}

		else if(cur_char == '<'){
			if(i+1 < line_length && line[i+1] == '<'){
				inittoken(my_list->list, my_list->length, RED_IN_HERE);
				my_list->length++;
				i++;
			}

			else{
				inittoken(my_list->list, my_list->length, RED_IN);
				my_list->length++;
			}
		}

		else if(cur_char == '>'){
			if(i+1 < line_length && line[i+1] == '>'){
				inittoken(my_list->list, my_list->length, RED_OUT_APP);
				my_list->length++;
				i++;
			}

			else{
				inittoken(my_list->list, my_list->length, RED_OUT);
				my_list->length++;
			}
		}

		//if cur_char hasn't been matched yet, it's a TEXT token
		else{

			bool escaped = false;//true if the preceeding character is an escape character

			//we'll record with the token an charr**argv and an int argc
			int argc = 0;
			int cur_pos = 0;
			int alloc_argc = 2;
			char**argv = malloc(alloc_argc*sizeof(char*));

			//the current argument in the argv list that we're writing to
			char*cur_arg = malloc((line_length+1)*sizeof(char));

			//while we're not at the end of the line, read until we find a non-escaped metachar
			while(i < line_length && (escaped || strchr(METACHAR, line[i]) == NULL)){

				//a non-escaped space ends the cur_arg
				if(isspace(line[i]) && !escaped){

					//if cur_pos == 0 then we must have just read a space
					//so do nothing
					if(cur_pos == 0){
						i++;
					}

					//else end the cur arg
					else{

						//realloc if necessary
						if(argc+1 >= alloc_argc){
							alloc_argc *=2;
							argv = realloc(argv, alloc_argc*sizeof(char*));
						}

						cur_arg[cur_pos] = '\0'; //null terminate the cur_arg

						argv[argc] = strdup(cur_arg); //copy cur_arg into the char**argv
						argc++;//update number of args
						i++;

						cur_pos = 0;//ready to write into the beginning of a new arg

						free(cur_arg); //free the cur_arg bc we used strdup

						cur_arg = malloc((line_length+1)*sizeof(char)); //allocate new space for the next arg
					}

				}
				
				//if not a space and not escaped
				else if(!escaped){

					//if curent character is an escape character then make escaped true
					if(line[i] == '\\'){
						escaped = true;
						i++;
					}

					//if current character is comment than make comment true and stop reading the line
					else if(line[i] == '#'){
						comment = true;
						break;
					}

					//else record the current character in cur_arg
					else{
						cur_arg[cur_pos] = line[i];
						cur_pos++;
						i++;
					}
				}
					
				//if escaped is true
				else{
					escaped = false;//escaped is now false

					//record the escaped character in cur_arg
					cur_arg[cur_pos] = line[i];
					cur_pos++;
					i++;
				}
			}

			//realloc argv if necessary (with enough space to add NULL at end)
			if(argc+1 >= alloc_argc){
				alloc_argc *=2;
				argv = realloc(argv, alloc_argc*sizeof(char*));
			}

			//if we read up to a metachar, decrement i so that the metachar gets processed
			//in the next loop of the while loop
			if(strchr(METACHAR, line[i]) != NULL){
				i--;
			}

			//if the curent argument hasn't been processed
			if(cur_pos != 0){
				cur_arg[cur_pos] = '\0'; //null terminate

				argv[argc] = strdup(cur_arg); //record in argv
				argc++;			
			}

			free(cur_arg); //we're done reading args

			argv[argc] = NULL; //NULL terminate argv string

			//if at least one arg was read
			if(argc != 0){
				//create token and point to argv and argc
				inittoken(my_list->list, my_list->length, TEXT);
				my_list->list[my_list->length].argv = argv;
				my_list->list[my_list->length].argc = argc;
				my_list->length++;
			}
			//if an argument wasn't read then free argv
			else{
				free(argv);
			}
		}

		//reallocate length list
		if(my_list->length >= my_list->alloc - 1){
			my_list->alloc *= 2;
			my_list->list = realloc(my_list->list, my_list->alloc*sizeof(TOKEN));
		}

		//break out of the while loop if a comment character has been read
		if(comment){
			break;
		}
	}

	//add NONE to the end of the token list to indicate the end
	inittoken(my_list->list, my_list->length, NONE);

	return 0; //arbitrary return

}

//Parser function
//calls the top recursive function and returns the value from that
CMD *parser(TOKEN_ARR*my_list){

	//pointer to an int that represents the current token being read
	int val = 0;
	int*position;
	position = &val;

	//flag to indicate an error (program will free memory and terminate)
	bool bul = false;
	bool*error_flag;
	error_flag = &bul;

	return command(my_list, position, error_flag);

}

//recursive parse functions listed from lowest to highest precedence

//local function
//[local]    = NAME=VALUE
int local(TOKEN_ARR*my_list, int*position, LOCAL_VAR*my_locals){

	//make sure we have a text type
	if(my_list->list[*position].type != TEXT){
		return 1;
	}

	//if all of the argvs have already been read
	int start = my_list->list[*position].beg;

	if(start >= my_list->list[*position].argc){
		return 1;
	}

	//the local string is just one argv
	char*localstr = my_list->list[*position].argv[start];

	//if not a valid local, interpret as argv instead
	if(strchr(localstr, '=') == NULL || strlen(localstr) < 2 || strchr("0123456789", localstr[0]) != NULL){
		return 1;
	}

	//else read the local
	else{

		//two strings, one for variable name and one for value
		char* varstr = malloc(strlen(localstr)*sizeof(char));
		char* valstr = malloc(strlen(localstr)*sizeof(char));

		int varindex = 0;
		int valindex = 0;

		int index = 0;

		//until we get to the equals sign
		while(localstr[index] != '='){

			//check that its a valid variable character
			if(strchr(VARCHR, localstr[index]) == NULL){
				free(varstr);
				free(valstr);
				return 1;
			}

			//copy into variable string
			varstr[varindex] = localstr[index];
			varindex++;
			index++;
		}

		//null terminate variable string
		varstr[varindex] = '\0';
		index++;

		//read the rest of the string into value
		for(;index < strlen(localstr); index++){
			valstr[valindex] = localstr[index];
			valindex++;
		}

		//and null terminate it
		valstr[valindex] = '\0';

		//realloc struct if necessary
		if(my_locals->num >= my_locals->alloc){
			my_locals->alloc*=2;
			my_locals->Vars = realloc(my_locals->Vars, my_locals->alloc*sizeof(char*));
			my_locals->Vals = realloc(my_locals->Vals, my_locals->alloc*sizeof(char*));
		}

		//pass info to struct
		my_locals->Vars[my_locals->num] = varstr;
		my_locals->Vals[my_locals->num] = valstr;

		my_locals->num++;
	}

	//we've read the first argv of the current token
	my_list->list[*position].beg++;

	//increment position if this was the last argv
	if(my_list->list[*position].beg == my_list->list[*position].argc){
		*position = *position + 1;
	}

	return 0;
}

//redirect function
//[redirect] = [red_op] FILENAME
int redirect(TOKEN_ARR*my_list, int*position, REDIRECT_VAR*my_redirects, bool*error_flag, bool*from_pipe){

	int type = my_list->list[*position].type;//record the type of the current token

	//only process if its a redirect
	if(RED_OP(type)){

		//if in type
		if(type == RED_IN || type == RED_IN_HERE){
			
			//if command already has an in redirect (error)
			if(my_redirects->fromtype != NONE || *from_pipe == true){
				fprintf(stderr, "parsley: two input redirects\n");
				*error_flag = true;
				return 2;
			}

			else{
					
				//make sure there's a filename to be read
				if(my_list->list[*position + 1].type != TEXT){
					fprintf(stderr, "parsley: missing filename\n");
					*error_flag = true;
					return 2;
				}

				//record redirect type
				my_redirects->fromtype = type;

				//increment *position to read filename
				*position = *position +1;

				//HERE doc
				if(type == RED_IN_HERE){

					//the keyword is the first arg
					char* keyword = strdup(my_list->list[*position].argv[0]);

					//duplicate the keyword to make a version with a new line at the end
					char* keyword_newline = malloc((strlen(keyword)+2)*sizeof(char));
					strcpy(keyword_newline, keyword);
					strcat(keyword_newline, "\n");

					//variables for getline
					size_t size = 60*sizeof(char);
					size_t *n = malloc(sizeof(size_t));
					*n = size;
					char*buffer = malloc(*n);
					char**lineptr;
					lineptr = &buffer;

					bool firstline = true; //if its the first line read we need to use strdup instead of strcat

					//while reading lines
					while(getline(lineptr, n, stdin) != -1){

						//if line matches the keyword
						if(strcmp(*lineptr, keyword) == 0 || strcmp(*lineptr, keyword_newline) == 0){
							
							//if keyword is the first line read, fromfile should be null
							if(firstline){
								my_redirects->fromfile = strdup("\0");
								firstline = false;
							}
							break;
						}

						//if its the first line read use strdup to copy line to formfile
						else if(firstline){
							my_redirects->fromfile = strdup(*lineptr);
							firstline = false;
						}

						//else realloc fromfile and use strcat
						else{
							my_redirects->fromfile = realloc(my_redirects->fromfile, \
								(strlen(my_redirects->fromfile)+strlen(*lineptr) + 3)*sizeof(char));
							strcat(my_redirects->fromfile,  *lineptr);

							if(strchr(*lineptr, '\n') == NULL){
								strcat(my_redirects->fromfile,  "\n");
							}

						}
					}

					free(n);
					free(buffer);
					free(keyword);
					free(keyword_newline);
				}

				//if not a HERE doc, just copy arg to fromfile
				else{
					my_redirects->fromfile = strdup(my_list->list[*position].argv[0]);
				}

				//we've read the beginning of the current token
				my_list->list[*position].beg++;

				//if this was the only argv of the current token, increment *position again
				if(my_list->list[*position].beg == my_list->list[*position].argc){
					*position = *position + 1;
				}

			}
		}

		//else its an out redirect
		else{

			//make sure command doesn't already have out redirect
			if(my_redirects->totype != NONE){
				fprintf(stderr, "parsley: two output redirects\n");
				*error_flag = true;
				return 2;
			}

			else{
				
				//if missing filename (error)
				if(my_list->list[*position+1].type != TEXT){
					fprintf(stderr, "parsley: missing filename\n");
					*error_flag = true;
					return 2;
				}

				//copy info to struct
				my_redirects->totype = type;

				*position = *position +1;

				my_redirects->tofile = strdup(my_list->list[*position].argv[0]);

				my_list->list[*position].beg++;

				//update *position if filename was the only argv
				if(my_list->list[*position].beg == my_list->list[*position].argc){
					*position = *position + 1;
				}
			}
		}

		return 0; //return 0 if something was processed
	}

	return 1; //return 1 if not
}


//prefix function
//[prefix]   = [local] / [redirect] / [prefix] [local] / [prefix] [redirect]
int prefix(TOKEN_ARR*my_list, int*position, LOCAL_VAR*my_locals, REDIRECT_VAR*my_redirects, bool*error_flag, bool*from_pipe){
		
	bool success = true;

	//while locals and redirects are still being processed
	while(success){

		//each loop must read either a local or redirect
		success = false;

		//0 returned from local if a local was read
		if(local(my_list, position, my_locals) == 0){
			success = true;
		}

		//if a local was not read try reading a redirect
		else if(RED_OP(my_list->list[*position].type)){

			int out = redirect(my_list, position, my_redirects, error_flag, from_pipe);

			if(out == 0){
				success = true;
			}

			if(out == 2){
				*error_flag = true;
				return 2;
			}
		}

		//if didn't read a local or redirect, break
		if(!success){
			break;
		}
	}

	return 0;
}


//suffix function
//[suffix]   = TEXT / [redirect] / [suffix] TEXT / [suffix] [redirect]
int suffix(TOKEN_ARR*my_list, int*position, REDIRECT_VAR*my_redirects, ARGV_VAR*my_argvs, bool*error_flag, bool*from_pipe){

	//while reading non null tokens
	while(my_list->list[*position].type != NONE){

		//if it's a redirect type
		if(RED_OP(my_list->list[*position].type)){
			
			//call redirect
			int out = redirect(my_list, position, my_redirects, error_flag, from_pipe);

			if(out == 2){
				*error_flag = true;
				return 2;
			}
		}

		//if TEXT
		else if(my_list->list[*position].type == TEXT){

			//process the text

			//beginning with unread argvs of current token (some argvs were read as locals or redirects in prefix)
			for(int i=my_list->list[*position].beg; i<my_list->list[*position].argc; i++){

				//realloc if necessary
				if(my_argvs->argc >= my_argvs->alloc){
					my_argvs->alloc*=2;
					my_argvs->argv = realloc(my_argvs->argv, my_argvs->alloc*sizeof(char*));
				}

				//copy argv[i] to my struct
				my_argvs->argv[my_argvs->argc] = strdup(my_list->list[*position].argv[i]);
				my_argvs->argc++;
			}

			//update *position to read the next redirect if present
			*position = *position +1;
		}

		else{
			break;
		}
	}

	//decrement position because we've read too far
	*position = *position -1;
	return 0;
}

//simple function
//[simple]   = TEXT / [prefix] TEXT / TEXT [suffix] / [prefix] TEXT [suffix]
CMD* simple(TOKEN_ARR*my_list, int*position, bool*error_flag, bool*from_pipe){

	//structs to keep chack of locals, redirects, and argvs
	LOCAL_VAR* my_locals = malloc(sizeof(LOCAL_VAR));
	initlocals(my_locals);

	REDIRECT_VAR* my_redirects = malloc(sizeof(REDIRECT_VAR));
	initredirects(my_redirects);

	ARGV_VAR* my_argvs = malloc(sizeof(ARGV_VAR));
	initargvs(my_argvs);

	//simples begin with a prefix
	prefix(my_list, position, my_locals, my_redirects, error_flag, from_pipe);

	//make sure there is a command
	if(*error_flag == false && my_list->list[*position].type != TEXT &&	\
			(my_locals->num != 0 || my_redirects->totype != NONE || \
			my_redirects->fromtype != NONE)){

		fprintf(stderr, "parsley: null command\n");
		*error_flag = true;
	}

	//if still reading the TEXT
	if(my_list->list[*position].type == TEXT && *error_flag == false){

		//then call suffix (all TEXT in simple will be recorded as the TEXT in suffix)
		suffix(my_list, position, my_redirects, my_argvs, error_flag, from_pipe);

		//if no errors
		//make a simple CMD and pass struct info
		if(*error_flag == false){
			CMD* node = mallocCMD(SIMPLE, NULL, NULL);

			//we'll pass our own char* so we don't need the allocated one
			free(node->argv);

			//realloc if necessary
			if(my_argvs->argc >= my_argvs->alloc){
				my_argvs->alloc*=2;
				my_argvs->argv = realloc(my_argvs->argv, my_argvs->alloc*sizeof(char*));
			}

			//add NULL to the end of argvs
			my_argvs->argv[my_argvs->argc] = NULL;

			//pass info to node
			node->argc = my_argvs->argc;
			node->argv = my_argvs->argv;
				
			//pass locals info to node if locals are present
			if(my_locals->num != 0){
				node->nLocal = my_locals->num;
				node->locVar = my_locals->Vars;
				node->locVal = my_locals->Vals;

				free(my_locals);
			}

			//if no locals, free the whole struct
			else{
				freelocals(my_locals);
			}

			//pass redirect info to node if present
			if(my_redirects->fromtype !=NONE){
				node->fromType = my_redirects->fromtype;
				node->fromFile = my_redirects->fromfile;
			}

			if(my_redirects->totype !=NONE){
				node->toType = my_redirects->totype;
				node->toFile = my_redirects->tofile;
			}

			//free struct pointers (other malloced info has been passed to node)
			free(my_redirects);
			free(my_argvs);
			
			return node;
		}
	}

	//free entire structs if no node was created
	freelocals(my_locals);
	freeredirects(my_redirects);
	freeargvs(my_argvs);
	return NULL;
}


//subcmd function
//[subcmd]   = ([command]) / [prefix] ([command]) / ([command]) [redList] / [prefix] ([command]) [redList]
CMD* subcmd(TOKEN_ARR*my_list, int*position, bool*error_flag, bool*from_pipe){

	//these structs will record locals and redirects for the current command
	LOCAL_VAR* my_locals = malloc(sizeof(LOCAL_VAR));
	initlocals(my_locals);

	REDIRECT_VAR* my_redirects = malloc(sizeof(REDIRECT_VAR));
	initredirects(my_redirects);

	CMD* com = NULL;

	//if the current position is not the beginning of a subcmd
	//then there must be a prefix beforehand to process
	if(my_list->list[*position].type != PAR_LEFT){

		prefix(my_list, position, my_locals, my_redirects, error_flag, from_pipe);

	}

	//make sure we're now at the beginning of a subcommand
	if(*error_flag == false && my_list->list[*position].type == PAR_LEFT){

		*position = *position + 1;

		//call command on the position following the left paren
		com = command(my_list, position, error_flag);

		//make sure a right paren is after the subcommand
		*position = *position + 1;

		if(*error_flag == false && my_list->list[*position].type != PAR_RIGHT){

			fprintf(stderr, "parsley: command and subcommand\n");
			*error_flag = true;
		}

		else{

			int index = 1;

			//check that there aren't too subcommands next to eachother (error)
			while(!CMD_END(my_list->list[*position + index].type)){

				if(my_list->list[*position + index].type == PAR_LEFT){
					fprintf(stderr, "parsley: command and subcommand\n");
					*error_flag = true;
				}
				index++;
			}

			//if we still haven't had an error
			if(*error_flag == false){

				//if there's a redirection after the subcmd
				if(RED_OP(my_list->list[*position+1].type)){

					*position = *position + 1;

					//continue reading while there is still a redirect
					while(*error_flag == false && RED_OP(my_list->list[*position].type)){
						redirect(my_list, position, my_redirects, error_flag, from_pipe);
					}

					//manual fix of a feature in redirect where *position is incremented if 
					//all of its information has been read as the filename for the redirect
					//that feature is not wanted here (it will mess up pipeline or andor if *position is already incremented)
					if(my_list->list[*position - 1].type == TEXT && my_list->list[*position - 1].argc == my_list->list[*position - 1].beg){
						*position = *position -1;
					}
				}

				//if all is still good make the subcmd node
				if(*error_flag == false){

					CMD* node = mallocCMD(SUBCMD, com, NULL);

					//if there are local variables pass that info
					if(my_locals->num != 0){

						node->nLocal = my_locals->num;
						node->locVar = my_locals->Vars;
						node->locVal = my_locals->Vals;
						free(my_locals);
					}

					//if not free the whole struct
					else{
						freelocals(my_locals);
					}

					//pass redirect info
					if(my_redirects->fromtype !=NONE){
						node->fromType = my_redirects->fromtype;
						node->fromFile = my_redirects->fromfile;
					}

					if(my_redirects->totype !=NONE){
						node->toType = my_redirects->totype;
						node->toFile = my_redirects->tofile;
					}

					free(my_redirects);
					
					return node;
				}
			}

		}
	}

	//else if error_flag was false then the paren was missing
	else{
		if(*error_flag == false){
			fprintf(stderr, "parsley: command and subcommand\n");
			*error_flag = true;
		}
	}

	//if no errors, com is returned above as part of the node
	if(com != NULL){
		freeCMD(com);
	}

	//free the structs
	freelocals(my_locals);
	freeredirects(my_redirects);
	return NULL;
}

//stage function
//[stage]    = [simple] / [subcmd]
CMD* stage(TOKEN_ARR*my_list, int*position, bool*error_flag, bool*from_pipe){

	bool sub_peak = false; //bool to look forward and see if there's a subcommand as part of the following command

	int index = 0;

	//peak forward in the list and set sub_peak to true if there's a left paren before the next command separator
	while(!CMD_END(my_list->list[*position + index].type)){

		if(my_list->list[*position + index].type == PAR_LEFT){
			sub_peak = true;
		}

		index++;
	}

	//if there is a paren, the next thing should be processed by subcmd
	if(sub_peak){
		CMD* sub = subcmd(my_list, position, error_flag, from_pipe);
		return sub;
	}

	//otherwise its a simple
	else{
		CMD* sim = simple(my_list, position, error_flag, from_pipe);
		return sim;
	}

	return NULL;
}

//pipeline function
//[pipeline] = [stage] / [pipeline] | [stage]
CMD* pipeline(TOKEN_ARR*my_list, int*position, bool* error_flag){

	//from_pipe will be passed downward to indicate that there should be no input redirect
	//in the command that follows a pipe
	bool bul = false;
	bool*from_pipe;
	from_pipe = &bul;

	CMD* S = stage(my_list, position, error_flag, from_pipe);

	//while reading pipes
	while(*error_flag ==false && S != NULL && my_list->list[*position+1].type == PIPE){

		*position = *position + 1;

		//check if missing a command
		if(my_list->list[*position+1].type == NONE || \
				my_list->list[*position+1].type == PIPE || \
				my_list->list[*position+1].type == SEP_BG || \
				my_list->list[*position+1].type == SEP_END || \
				my_list->list[*position+1].type == SEP_AND || \
				my_list->list[*position+1].type == SEP_OR || \
				my_list->list[*position+1].type == PAR_RIGHT){

			fprintf(stderr, "parsley: null command\n");
			*error_flag = true;
		}

		//else continue pipe recursion
		else{

			*position = *position + 1;

			*from_pipe = true;

			CMD* T = stage(my_list, position, error_flag, from_pipe);

			if(T == NULL){
				return NULL;
			}

			else{

				//check if the left child had an output redirect (error)
				if(S->toType != NONE){
					fprintf(stderr, "parsley: two output redirects\n");
					*error_flag = true;
				}

				//make this regardless, then we'll free it if the above error occured, thereby freeing T too
				CMD* node = mallocCMD(PIPE, S, T);
				S = node;
			}
		}
	}

	//free CMDs if error has occured
	if(*error_flag == true && S != NULL){
		freeCMD(S);
		S =NULL;
	}

	return S;
}

//and-or function
//[and-or]   = [pipeline] / [and-or] && [pipeline] / [and-or] || [pipeline]
CMD* andor(TOKEN_ARR*my_list, int*position, bool*error_flag){

	//structure is very similar to sequence

	CMD* P = pipeline(my_list, position, error_flag);

	int cur_type;

	//while still reading && or ||
	while(*error_flag == false && P != NULL && (my_list->list[*position+1].type == SEP_AND || my_list->list[*position+1].type == SEP_OR)){

		*position = *position + 1;

		cur_type = my_list->list[*position].type;

		//redundant, cur_type is SEP_AND or SEP_OR
		if(cur_type != NONE){

			//make sure a command is not missing
			if(my_list->list[*position+1].type == NONE || \
				my_list->list[*position+1].type == PIPE || \
				my_list->list[*position+1].type == SEP_BG || \
				my_list->list[*position+1].type == SEP_END || \
				my_list->list[*position+1].type == SEP_AND || \
				my_list->list[*position+1].type == SEP_OR || \
				my_list->list[*position+1].type == PAR_RIGHT){

				fprintf(stderr, "parsley: null command\n");
				*error_flag = true;
			}

			//else continue the recursion
			else{

				*position = *position + 1;

				CMD* I = pipeline(my_list, position, error_flag);

				if(I == NULL){
					return NULL;
				}

				else if(cur_type == SEP_AND){
					CMD* node = mallocCMD(SEP_AND, P, I);
					P = node;
				}
				else{
					CMD* node = mallocCMD(SEP_OR, P, I);
					P = node;
				}
			}
		}
	}

	//make sure to free things if an error has occured
	if(*error_flag == true && P != NULL){
		freeCMD(P);
		P = NULL;
	}

	return P;
}

//sequence function
//[sequence] = [and-or] / [sequence] ; [and-or] / [sequence] & [and-or]
CMD* sequence(TOKEN_ARR*my_list, int*position, bool*error_flag){

	CMD* A = andor(my_list, position, error_flag);

	int cur_type;

	//create nodes while commands are separated by ; or &
	while(*error_flag == false && A != NULL && (my_list->list[*position+1].type == SEP_END || my_list->list[*position+1].type == SEP_BG)){

		//if nothing follows (or is the end of a subcmd) the SEP_BG or SEP_END will be processed by command
		if(my_list->list[*position+2].type == NONE || my_list->list[*position+2].type == PAR_RIGHT){
			return A;
		}

		//else the SEP_END or SEP_BG is between and-or's
		//create a SEP_END or SEP_BG node with A and the left child and N as the right child
		else{
			cur_type = my_list->list[*position+1].type;

			*position = *position + 2;

			CMD* N = andor(my_list, position, error_flag);

			//if there's an error with the recent call, we need to free the CMD A from previously
			if(*error_flag == true){
				freeCMD(A);
				A = NULL;
			}

			else if(cur_type == SEP_END){
				CMD* node = mallocCMD(SEP_END, A, N);
				A = node;
			}
			else{
				CMD* node = mallocCMD(SEP_BG, A, N);
				A = node;
			}
		}
	}

	return A;
}

//Command function
//[command]  = [sequence] / [sequence] ; / [sequence] &
CMD* command(TOKEN_ARR*my_list, int*position, bool*error_flag){

	CMD* seq = NULL;

	//if missing a command right at the beginning
	if(my_list->list[*position].type == SEP_END ||\
		my_list->list[*position].type == SEP_BG ||\
		my_list->list[*position].type == PIPE ||\
		my_list->list[*position].type == SEP_AND ||\
		my_list->list[*position].type == SEP_OR){

		fprintf(stderr, "parsley: null command\n");
		*error_flag = true;
	}

	else{
		seq = sequence(my_list, position, error_flag);//all commands contain sequences

		//a command can be followed by a SEP_END or SEP_BG
		if(*error_flag == false && (my_list->list[*position+1].type == SEP_END || my_list->list[*position+1].type == SEP_BG)){

			//if so, increment position and make a node of the corresponding type
			//SEP_END and SEP_BG have NULL right children

			*position = *position + 1;

			if(my_list->list[*position].type == SEP_END){
				CMD* node = mallocCMD(SEP_END, seq, NULL);
				seq = node;
			}

			else{
				CMD* node = mallocCMD(SEP_BG, seq, NULL);
				seq = node;
			}
		}
	}

	return seq;
}


//functions to allocate and free structs----------------------------------------

void inittoken(TOKEN*list, int index, int type){
	list[index].type = type;
	list[index].argv = NULL;
	list[index].argc = 0;
	list[index].beg = 0;
}

void initlocals(LOCAL_VAR*my_locals){
	my_locals->alloc = 2;
	my_locals->num = 0;

	my_locals->Vars = malloc(my_locals->alloc * sizeof(char*));
	my_locals->Vals = malloc(my_locals->alloc * sizeof(char*));
}
void freelocals(LOCAL_VAR*my_locals){

	for(int i = 0; i < my_locals->num; i++){
		free(my_locals->Vars[i]);
		free(my_locals->Vals[i]);
	}
	free(my_locals->Vars);
	free(my_locals->Vals);
	free(my_locals);
}

void initredirects(REDIRECT_VAR*my_redirects){
	my_redirects->fromtype = NONE;
	my_redirects->totype = NONE;
}
void freeredirects(REDIRECT_VAR*my_redirects){
	if(my_redirects->fromtype != NONE){
		free(my_redirects->fromfile);
	}
	if(my_redirects->totype !=NONE){
		free(my_redirects->tofile);
	}

	free(my_redirects);
}

void initargvs(ARGV_VAR*my_argvs){
	my_argvs->argc = 0;
	my_argvs->alloc = 2;
	my_argvs->argv = malloc(my_argvs->alloc*sizeof(char*));
}
void freeargvs(ARGV_VAR*my_argvs){

	for(int i = 0; i < my_argvs->argc; i++){
		free(my_argvs->argv[i]);
	}

	free(my_argvs->argv);
	free(my_argvs);
}


//linked list stack functions---------------------------------------------------

/*ATTRIBUTION: linked list code from "Notes on Data Structures and Programming Techniques (CPSC 223, Spring 2018)" by James Aspnes*/
void stackPush(STACK *s, int value){
	STACK e;
	e = malloc(sizeof(struct element));
	/*assert(e);*/
	e->val = value;
	e->parent = *s;
	*s = e;
}

int stackPop(STACK *s){
	int ret;
	struct element *e;
	//assert(!stackEmpty(s));
	ret = (*s)->val;
	/* patch out first element */
	e = *s;
	*s = e->parent;
	free(e);
	return ret;
}

int stackEmpty(const STACK *s){
	return (*s == 0);
}
/*END ATTRIBUTION*/