/*Chris West, cgw29*/

#define _GNU_SOURCE
#include "/c/cs323/Hwk4/code.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>


//bit-fields: define bits of a struct

//string table struct: using struct of arrays method
typedef struct table{
	uint32_t* prefandchar; //bitpacked prefix and char
	int* next_code; //chains for hash table and/or helper array for pruning
	int num_entries; //number of filled entries in the table
	int curbit; //alloced memory (length of above arrays) is (1 << curbit)
} TABLE;

//hash table for encode
typedef struct hash_table{
	int* list; //array of heads of chains - each head is an (int)code that corresponds to an index in the string table
	int num_chains; //length of above list
	int alloc_num_chains; //alloced length usually equals length. not equal if pruning shrinks the table
} HASH_TABLE;


//stack for use with decode
//ATTRIBUTION: Stack code from James Aspnes' "Notes on Data Structures and Programming Techniques"
struct elt {
	struct elt *next;
	int value;};

typedef struct elt *Stack;
#define STACK_EMPTY (0)

void stackPush(Stack *s, int value){
	struct elt *e;
	e = malloc(sizeof(struct elt));
	assert(e);
	e->value = value;
	e->next = *s;
	*s = e;}

int stackEmpty(const Stack *s){
	return (*s == 0);}

int stackPop(Stack *s){
	int ret;
	struct elt *e;
	assert(!stackEmpty(s));
	ret = (*s)->value;
	/* patch out first element */
	e = *s;
	*s = e->next;
	free(e);
	return ret;}
//END ATTRIBUTION


//helper functions for string table
TABLE* initialize_table(bool encode, bool pruning);
int add_entry(TABLE*table, int code, int prefix, char last, bool encode);
void embiggen_table(TABLE* mytable, bool encode,  bool pruning);
int prune_table(TABLE* mytable, bool encode, bool pruning, int C);
void free_table(TABLE* table, bool encode, bool pruning);

//helper functions for hash table
HASH_TABLE* create_hash(int curbit);
void hash_the_table(TABLE* table, HASH_TABLE* myhash);
void add_hash_entry(HASH_TABLE* myhash, TABLE* mytable, int code, int hash);
void rehash(TABLE* mytable, HASH_TABLE* myhash);
int HASH(int prefix, char charac, int size);

//helper functions for bitpacked prefix and char
uint32_t pack(int C, int K);
int unpack_c(uint32_t data);
char unpack_k(uint32_t data);

//encode function
int encode(int maxbits, bool pruning);

//decode function
int decode(void);


//MAIN------------------------------------------------
//processes argv's, checks that command line is valid depending on argv[0]
//calls encode or decode as indicated by argv[0]
int main(int argc, char** argv){
	
	// Use small static buffers
	static char bin[64], bout[64];
    setvbuf (stdin,  bin,  _IOFBF, 64);
    setvbuf (stdout, bout, _IOFBF, 64);

	setenv ("STAGE", "3", 1);

	//to check against encode / decode
	char* progname = basename(argv[0]);

	//if argv[0] is encode
	if(strcmp(progname, "encode") == 0){

		//pruning false by default
		bool pruning = false;

		//maxbits 12 by default
		int maxbits = 12;

		//read rest of the argv's
		//last occurence of -p or -m is the one used
		for(int i = 1; i < argc; i++){

			//if -m MAXBITS
			if(strcmp(argv[i], "-m")==0){

				//-m must be followed by MAXBITS
				if(i == argc-1){
					fprintf(stderr, "encode: [-m MAXBITS]\n");
					return 1;
				}

				//read MAXBITS in argv[i+1]
				i++;
				maxbits = atoi(argv[i]);
			}

			//-p means pruning is true
			else if(strcmp(argv[i], "-p")==0){
				pruning = true;
			}

			//not valid if not -p or -m MAXBITS
			else{
				fprintf(stderr, "encode: invalid command line argument\n");
				return 1;
			}
		}

		//ensure maxbits in valid range
		if(maxbits > 20 || maxbits <= 8){
			maxbits = 12;
		}

		//execute encode
		encode(maxbits, pruning);
	}

	//if argv[0] is decode
	else if(strcmp(progname, "decode") == 0){

		//decode cannot be followed by any command line arguments
		if (argc != 1){
			fprintf(stderr, "decode: unexpected command line argument\n");
			return 1;
		}

		//execute decode
		decode();
	}

	return 0;
}

//ENCODE------------------------------------------------
int encode(int maxbits, bool pruning){
	
	//flag to send to helper functions	
	bool encode = true;

	//starting curbit
	int curbit = 9;

	//initialize hash and string tables
	TABLE* mytable = initialize_table(encode, pruning);
	HASH_TABLE* myhash = create_hash(mytable->curbit);
	hash_the_table(mytable, myhash);
	
	//variable initializations
	int C = 0; //EMPTY = 0
	int read_K; //int read from getchar
	char K; //read_k converted to a char
	int code; //code of (prefix, char)
	int hash; //hash of (prefix, char)
	bool foundit; //true if (prefix, char) is in table
	bool full = false; //true if table is full

	//first code sent is the maxbits value
	putBits(curbit, maxbits);

	//second code sent indicates if pruning is enabled
	if(pruning){putBits(curbit, 1);}
	else{putBits(curbit, 0);}

	//main encode loop
	while((read_K = getchar()) != EOF){

		//convert read result to a char
		K = (char)read_K;

		//initialize found_it
		foundit = false;

		//get hash result of (prefix, char)
		hash = HASH(C, K, myhash->num_chains);

		//follow ptr until ptr == 0, end of chain
		int ptr = myhash->list[hash];
		while(ptr != 0){

			//if current entry matches (pref, char)
			if(unpack_c(mytable->prefandchar[ptr]) == C && unpack_k(mytable->prefandchar[ptr]) == K){
				foundit = true;
				code = ptr;
				break;
			}

			//else follow next_code to next entry in chain
			ptr = mytable->next_code[ptr];
		}

		//if (pref, char) already in table
		if(foundit){
			C = code;
		}

		//else add (pref, char) to table
		else{	

			//send previous code
			putBits(curbit, C);

			//if table is full
			if(mytable->num_entries + 2 >= (1 << mytable->curbit)){

				//and we're at maxbits
				if(curbit == maxbits){

					//and pruning is enabled, then prune
					if(pruning){

						//send 11111... to indicate that we're pruning
						putBits(curbit, (1 << curbit) - 1);

						//prune the table and update C
						C = prune_table(mytable, encode, pruning, C);
												
						//reconfigure the hash table
						rehash(mytable, myhash);	

						//update curbit (may have decreased)
						curbit = mytable->curbit;
					}

					//if not pruning, or if table still full after pruning, then table is full
					if(mytable->num_entries+2 >= (1 << mytable->curbit)){
						full = true;
					}
				}

				//if not at maxbits
				else{
					
					//send 0 to indicate we're growing
					putBits(curbit, 0);

					//update curbit
					curbit++;

					//grow the table
					embiggen_table(mytable, encode, pruning);

					//reconfigure the hash table
					rehash(mytable, myhash);
				}

				//hash table has changed, so hash of (pref, char) needs to be updated
				hash = HASH(C, K, myhash->num_chains);
			}

			//if table is not full
			if(!full){

				//add (pref, char) to table
				int newcode = add_entry(mytable, mytable->num_entries, C, K, encode);
				
				//update hash table
				add_hash_entry(myhash, mytable, newcode, hash);
			}

			//set C equal to the code for (0, K)
			C = (int)K;

			//(int)K might be negative, get into range (1, 256)
			if(C>= 0){
				C = C + 1;
			}
			else{
				C = C + 257;
			}
		}
	}

	//send final code
	if(C != 0){	
		putBits(curbit, C);
	}

	flushBits();

	//free alloced memory
	free_table(mytable, encode, pruning);
	free(myhash->list);
	free(myhash);

	return 0;
}


//DECODE------------------------------------------------
int decode(void){

	//not in encode, don't allocate memory for hash table chains, unless needed for pruning
	bool encode = false;
	
	//curbit begins at 9
	int curbit = 9;

	//read maxbits and check that it's valid
	int maxbits = getBits(curbit);
	if(maxbits <= 8 || maxbits > 20){
		fprintf(stderr, "decode: invalid code recieved\n");
		return 1;
	}

	//variables for decode while loop
	int oldC = 0;
	int newC;
	int C;
	char finalK;
	char K;

	//indicate if string table is full
	bool full = false;

	//inidicate if pruning is enabled (allocate next_code array in string table if so)
	bool pruning = false;

	//second code sent by encode is 1 if pruning, 0 otherwise
	C = getBits(curbit);
	if(!(C == 0 || C == 1)){
		fprintf(stderr, "decode: invalid code recieved\n");
		return 1;
	}
	if(C == 1){
		pruning = true;
	}

	//initialize string table
	TABLE* mytable = initialize_table(encode, pruning);

	//initialize stack variables
	Stack mystack;
	mystack = STACK_EMPTY;

	//while reading lines
	while((newC = C = getBits(curbit)) != EOF){

		//if code out of bounds
		if( C < 0 || C > (1 << curbit)){
			fprintf(stderr, "decode: invalid code recieved\n");
			free_table(mytable, encode, pruning);
			return 1;
		}

		//special code GROW = 0
		if(C == 0){
			curbit++;
			embiggen_table(mytable, encode, pruning);
			continue;
		}

		//special code PRUNE = 1111...
		if(C == (1 << curbit) - 1){

			//prune the table and update oldC
			oldC = prune_table(mytable, encode, pruning, oldC);

			//update curbit (may have decreased)
			curbit = mytable->curbit;
			continue;
		}

		//if code is not in table
		if(C >= mytable->num_entries){

			//push finalK
			stackPush(&mystack, finalK);
			C = oldC;
		}

		//follow prefix until 0
		while(unpack_c(mytable->prefandchar[C]) != 0){
			//push char to stack
			stackPush(&mystack, unpack_k(mytable->prefandchar[C]));
			//follow the prefix
			C = unpack_c(mytable->prefandchar[C]);
		}

		//print chars from stack
		finalK = unpack_k(mytable->prefandchar[C]);
		printf("%c", finalK);
		while(!stackEmpty(&mystack)){
			K = stackPop(&mystack);
			printf("%c", K);
		}

		//if we need to add to the table
		if(oldC != 0){

			//check if table is full
			if(mytable->num_entries + 2 >= (1 << mytable->curbit)){
				full = true;
			}

			//if table is not full, add the entry
			if(!full){
				add_entry(mytable, mytable->num_entries, oldC, finalK, encode);
			}
		}

		//remember the previus C
		oldC = newC;
	}

	//free alloced memory
	free_table(mytable, encode, pruning);

	return 0;
}


//returns a TABLE* with first 256 entries initialized
TABLE* initialize_table(bool encode, bool pruning){

	//create mytable
	TABLE* mytable = malloc(sizeof(TABLE));

	//always start with curbit = 9
	mytable->curbit = 9;

	//malloc for bitpacked prefix and char
	mytable->prefandchar = malloc((1 << mytable->curbit)*sizeof(uint32_t));
		
	//only malloc next_code if using in encode or if we'll be pruning
	if(encode || pruning){
		mytable->next_code = malloc((1 << mytable->curbit)*sizeof(int));
	}
		
	//EMPTY = 0
	mytable->prefandchar[0] = pack(0, 0);

	//only set next_code if in encode or if we'll be pruning
	if(encode || pruning){
		mytable->next_code[0] = 0;
	}

	//initialize all single char entries
	for(int i = 0; i < 256; i++){
		mytable->prefandchar[i+1] = pack(0, i);

		if(encode || pruning){
			mytable->next_code[i+1] = 0;
		}
	}
		
	//we've put in 257 entries to the table
	mytable->num_entries = 257;

	return mytable;
}

//add a (pref, char) entry at index code to table, return index code
int add_entry(TABLE*table, int code, int prefix, char last, bool encode){
	
	//put bitpacked (prefix, char) in table
	table->prefandchar[code] = pack(prefix, (int)last);

	//set next_code if in encode
	if(encode){
		table->next_code[code] = 0;
	}

	//update num_entries
	table->num_entries++;

	//return the code we put the new entry in
	return code;
}

//double the size of mytable
void embiggen_table(TABLE* mytable, bool encode, bool pruning){

	//increase curbit (table is of size 2^curbit)
	mytable->curbit++;

	//realloc prefix, char array
	mytable->prefandchar = realloc(mytable->prefandchar, (1 << mytable->curbit)*(sizeof(uint32_t)));
		
	//realloc next_code if in encode or if pruning
	if(encode || pruning){
		mytable->next_code = realloc(mytable->next_code, (1 << mytable->curbit)*(sizeof(int)));
		
		//make all entries zero, will be updated in rehash()
		for(int i = 0; i < (1 << mytable->curbit); i++){
			mytable->next_code[i] = 0;
		}
	}
}

//prune mytable, return the new location of the code C
int prune_table(TABLE* mytable, bool encode, bool pruning, int C){

	//use next_codes, these will need to be re-calculated anyway after pruning
	int* keys = mytable->next_code;

	//set all keys to 0, then set a key[i] equal to 1 if i is the prefix of another string
	for(int i = 1; i < mytable->num_entries; i++){
		keys[i] = 0;
		keys[unpack_c(mytable->prefandchar[i])] = 1;
	}

	//ensure entry C remains, will be the prefix of the soon-to-be-added entry
	keys[C] = 1;

	//the new prefix for an entry
	int prefix;

	//the old prefix for an entry
	int old_prefix;

	//the current number of entries in the new string table
	int new_num_entries = 257;

	//go through all entries that aren't the foundational ones
	for(int i = 257; i < mytable->num_entries; i++){

		//keep the entry if it is a prefix of another code
		if(keys[i] == 1){

			//get the old prefix
			old_prefix = unpack_c(mytable->prefandchar[i]);

			//if old prefix is > 256, it may have changed
			if(old_prefix >= 257){

				//keys holds the new prefixes, indexed by the old ones
				prefix = keys[old_prefix];
			}

			//else it hasn't changed
			else{
				prefix = old_prefix;
			}
				
			//the current entry will be readded at index new_num_entries
			//keys holds this information
			keys[i] = new_num_entries;

			//add the entry at the new location
			mytable->prefandchar[new_num_entries] = pack(prefix, (int)unpack_k(mytable->prefandchar[i]));

			//increment new_num_entries
			new_num_entries++;
		}
	}

	//remember the new location of C, to return later
	int new_C = keys[C];
		
	//update curbit in this loop
	int curbit = 9;
	while((1<<curbit) <= new_num_entries+2){
		curbit++;
	}

	//update the new curbit into the table
	mytable->curbit = curbit;
		
	//shorten alloced arrays if we can
	mytable->prefandchar = realloc(mytable->prefandchar, (1<<curbit)*sizeof(uint32_t));

	if(encode  || pruning){
		mytable->next_code = realloc(mytable->next_code, (1<<curbit)*sizeof(int));

		//re_initialize next_code so rehash will work
		for(int i = 0; i < (1<<curbit); i++){
			mytable->next_code[i] = 0;
		}
	}

	//update the num_entries in the table
	mytable->num_entries = new_num_entries;

	//return the new index for C
	return new_C;
}

//free table
void free_table(TABLE* table, bool encode, bool pruning){

	free(table->prefandchar);

	//only free next_code if it was alloced
	if(encode  || pruning){
		free(table->next_code);
	}

	free(table);
}

//returns an initialized hash table
HASH_TABLE* create_hash(int curbit){
	
	//initialize
	HASH_TABLE* myhash = malloc(sizeof(HASH_TABLE));

	//calculate num_chains
	myhash->num_chains = (1 << (curbit - 3)) - 1;

	//usually alloced_num_chains will equal num_chains. may be different after pruning
	myhash->alloc_num_chains = myhash->num_chains;

	//alloc memory for list of headers
	myhash->list = malloc(myhash->alloc_num_chains*sizeof(int));

	//initialize all headers as pointing to EMPTY = 0
	for(int i=0; i < myhash->num_chains; i++){
		myhash->list[i] = 0;
	}

	//return the hash table
	return myhash;
}

//takes a string table mytable and hash table myhash, and hashes the string table, storing the heads in myhash
void hash_the_table(TABLE* mytable, HASH_TABLE* myhash){
	
	//hash value of (pref, char)
	int hash;
	
	//for all the entries of the string table other than EMPTY = 0
	for(int i=1; i<mytable->num_entries; i++){
		
		//calulate the hash value
		hash = HASH(unpack_c(mytable->prefandchar[i]), unpack_k(mytable->prefandchar[i]), myhash->num_chains);
		
		//add the hash entry
		add_hash_entry(myhash, mytable, i, hash);
	}
}


//updates next_code in mytable and the list in myhash for the addition of an entry
void add_hash_entry(HASH_TABLE* myhash, TABLE* mytable, int code, int hash){
	
	//to follow down the hash chains
	int prev;
	int next;
			
	//if the header points to 0
	if(myhash->list[hash] == 0){

		//just point the header to the index of the new entry
		myhash->list[hash] = code;
	}

	//otherwise
	else{

		//follow the chain until we get to the end (= 0)
		//keep track of the previous index;
		prev = myhash->list[hash];
		next = mytable->next_code[myhash->list[hash]];
		while(next != 0){
			prev = next;
			next = mytable->next_code[next];
		}

		//add the new entry at the end of the chain
		mytable->next_code[prev] = code;
	}
}

//to be used after pruning or embiggen. updates myhash and hashes the table
void rehash(TABLE* mytable, HASH_TABLE* myhash){

	//skrink or grow num_chains if necessary
	myhash->num_chains = (1 << (mytable->curbit - 3)) - 1;

	//don't realloc unless needed (num_chains could have shrunk before and is now growing into already alloced space)
	if(myhash->alloc_num_chains < myhash->num_chains){
		myhash->alloc_num_chains = myhash->num_chains;
		myhash->list = realloc(myhash->list, myhash->alloc_num_chains*sizeof(int));
	}

	//initialize heads all to 0
	for(int i=0; i < myhash->num_chains; i++){
		myhash->list[i] = 0;
	}

	//and hash the table
	hash_the_table(mytable, myhash);
}

//hash (prefix, char) using the hash function in the spec. size = num_chains
int HASH(int prefix, char charac, int size){
	
	//convert charac to an int in range (0, 255)
	int pos_c = charac;
	if(pos_c < 0){
		pos_c = pos_c + 256;
	}

	//compute the hash
	int result = abs(((prefix << CHAR_BIT) | pos_c) % size);
	
	return result;
}


//prefix = C, char = K. return bitpacked (prefix, char)
uint32_t pack(int C, int K){

	//ensure K is in range (0, 255)
	if(K < 0){
		K = K + 256;
	}

	//pack C and K together
	uint32_t result = K | (C << 8);

	return result;
}

//upack a prefix
int unpack_c(uint32_t data){return (int)(0xFFFFFF00 & data) >> 8;}

//unpack a char
char unpack_k(uint32_t data){return (char)(0x000000FF & data);}