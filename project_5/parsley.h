// parse.h                                        Stan Eisenstat (09/16/20)
//
// Header file for command line parser used in parsley
// Bash version based on left-associative parse tree

#ifndef PARSE_INCLUDED
#define PARSE_INCLUDED          // parse.h has been #include-d

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <stdbool.h>

// A token is
//
// (1) a maximal contiguous sequence of non-whitespace printing characters (see
//     "man 3 isgraph") other than the metacharacters <, >, ;, &, |, (, and )
//     [a TEXT token];
//
// (2) a redirection symbol (<, <<, >, >>, 2>, 2>>, or &>);
//
// (3) a pipeline symbol (|);
//
// (4) a command operator (&& or ||);
//
// (5) a command terminator (; or &);
//
// (6) a left or right parenthesis [used to group commands into subcommands];

/////////////////////////////////////////////////////////////////////////////

// Token types used by parse()

enum {TEXT,             // Maximal contiguous sequence ... (as above)

      RED_IN,           // <    Redirect stdin to file
      RED_IN_HERE,      // <<   Redirect stdin to HERE document

      RED_OUT,          // >    Redirect stdout to file
      RED_OUT_APP,      // >>   Append stdout to file
      RED_OUT_ERR,      // &>   Redirect stdout and stderr to file

      RED_ERR,          // 2>   Redirect stderr to file
      RED_ERR_APP,      // 2>>  Append stderr to file

      PIPE,             // |

      SEP_AND,          // &&
      SEP_OR,           // ||

      SEP_END,          // ;
      SEP_BG,           // &

      PAR_LEFT,         // (
      PAR_RIGHT,        // )

      SIMPLE,           // Nontoken: CMD struct for simple command
      SUBCMD,           // Nontoken: CMD struct for subcommand
      NONE,             // Nontoken: Did not find a token
      ERROR,            // Nontoken: Encountered an error
};


// String containing all metacharacters that terminate TEXT tokens
#define METACHAR "<>;&|()"


// String containing all characters that may appear in variable names
#define VARCHR "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"


// Macro that checks whether a token is a redirection symbol
#define RED_OP(type) (type == RED_IN  || type == RED_IN_HERE || \
		      type == RED_OUT || type == RED_OUT_APP || \
		      type == RED_ERR || type == RED_ERR_APP || \
		      type == RED_OUT_ERR)

/////////////////////////////////////////////////////////////////////////////

// The syntax for a command is
//
//   [local]    = VARIABLE=VALUE
//   [red_op]   = < / << / > / >> / 2> / 2>> / &>
//   [redirect] = [red_op] FILENAME
//   [prefix]   = [local] / [redirect] / [prefix] [local] / [prefix] [redirect]
//   [suffix]   = TEXT / [redirect] / [suffix] TEXT / [suffix] [redirect]
//   [redList]  = [redirect] / [redList] [redirect]
//   [simple]   = TEXT / [prefix] TEXT / TEXT [suffix]
//                     / [prefix] TEXT [suffix]
//   [subcmd]   = ([command]) / [prefix] ([command]) / ([command]) [redList]
//                            / [prefix] ([command]) [redList]
//   [stage]    = [simple] / [subcmd]
//   [pipeline] = [stage] / [pipeline] | [stage]
//   [and-or]   = [pipeline] / [and-or] && [pipeline] / [and-or] || [pipeline]
//   [sequence] = [and-or] / [sequence] ; [and-or] / [sequence] & [and-or]
//   [command]  = [sequence] / [sequence] ; / [sequence] &
//
//   Note that FILENAME = TEXT.
//
// Aside: The grammar is ambiguous in the sense that in the input
//   % A=B
// parsley could treat the TEXT token A=B as an argument in a valid command or
// as a local in an invalid one.  Similarly, for the input
//   % A=B C=D E
// it could treat A=B and C=D as locals, or treat A=B as a local and C=D as an
// argument, or treat A=B and C=D as arguments; all of which result in valid
// commands.  To resolve the ambiguity we will assume that a TEXT token of the
// form VARIABLE=VALUE is always treated as a [local] if it can be.
//
// A command is represented as a tree of CMD structs containing its [simple]
// commands and the "operators" | (= PIPE), && (= SEP_AND), || (= SEP_OR),
// ; (= SEP_END), & (= SEP_BG), and SUBCMD.  The command tree is determined
// by (but is not equal to) the parse tree in the above grammar.
//
// The tree for a [simple] is a single struct of type SIMPLE that specifies its
// arguments (argc, argv[]); its local variables (nLocal, locVar[], locVal[]);
// and whether and where to redirect its standard input (fromType, fromFile),
// its standard output (toType, toFile), and its standard error (errType,
// errFile).  The left and right children are NULL.
//
// The tree for a [stage] is either the tree for a [simple] or a CMD struct of
// type SUBCMD (which may have local variables and redirection) whose left
// child is the tree representing a [command] and whose right child is NULL.
// Note that I/O redirection is associated with a [stage] (i.e., a [simple] or
// [subcmd]), but not with a [pipeline] (redirection for the first/last stage
// is associated with the stage, not the pipeline).
//
// The tree for a [pipeline] is either the tree for a [stage] or a CMD struct
// of type PIPE whose right child is a tree representing the last [stage] and
// whose left child is the tree representing the rest of the [pipeline].
//
// The tree for an [and-or] is either the tree for a [pipeline] or a CMD
// struct of type && (= SEP_AND) or || (= SEP_OR) whose left child is a tree
// representing an [and-or] and whose right child is a tree representing a
// [pipe-line].
//
// The tree for a [sequence] is either the tree for an [and-or] or a CMD
// struct of type ; (= SEP_END) or & (= SEP_BG) whose left child is a tree
// representing a [sequence] and whose right child is a tree representing an
// [and-or].
//
// The tree for a [command] is either the tree for a [sequence] or a CMD
// struct of type ; (= SEP_END) or & (= SEP_BG) whose left child is the tree
// representing a [sequence] and whose right child is NULL.

// Examples (where A, B, C, D, and E are [simple]):                          //
//                                                                           //
//                              Command Tree                                 //
//                                                                           //
//   < A B | C | D | E > F                     PIPE                          //
//                                            /    \                         //
//                                        PIPE      E >F                     //
//                                       /    \                              //
//                                   PIPE      D                             //
//                                  /    \                                   //
//                              <A B      C                                  //
//                                                                           //
//   A && B || C && D                   &&                                   //
//                                     /  \                                  //
//                                   ||    D                                 //
//                                  /  \                                     //
//                                &&    C                                    //
//                               /  \                                        //
//                              A    B                                       //
//                                                                           //
//   A ; B & C ; D || E ;                 ;                                  //
//                                      /                                    //
//                                     ;                                     //
//                                   /   \                                   //
//                                  &     ||                                 //
//                                 / \   /  \                                //
//                                ;   C D    E                               //
//                               / \                                         //
//                              A   B                                        //
//                                                                           //
//   (A ; B &) | (C || D) && E                 &&                            //
//                                            /  \                           //
//                                        PIPE    E                          //
//                                       /    \                              //
//                                    SUB      SUB                           //
//                                   /        /                              //
//                                  &       ||                               //
//                                 /       /  \                              //
//                                ;       C    D                             //
//                               / \                                         //
//                              A   B                                        //

typedef struct cmd {
  int type;             // Node type: SIMPLE, PIPE, SEP_AND, SEP_OR, SEP_END,
			//   SEP_BG, SUBCMD, or NONE (default)

  int argc;             // Number of command-line arguments
  char **argv;          // Null-terminated argument vector or NULL

  int nLocal;           // Number of local variable assignments
  char **locVar;        // Array of local variable names and the values to
  char **locVal;        //   assign to them when the command executes

  int fromType;         // Redirect stdin: NONE (default), RED_IN (<), or
			//   RED_IN_HERE (<<)
  char *fromFile;       // File to redirect stdin, contents of here document,
			//   or NULL (default)

  int toType;           // Redirect stdout: NONE (default), RED_OUT (>),
			//   RED_OUT_APP (>>), or  RED_OUT_ERR (&>)
  char *toFile;         // File to redirect stdout or NULL (default)

  int errType;          // Redirect stderr: NONE (default), RED_ERR (2>),
			//   RED_ERR_APP (2>>), or RED_OUT_ERR (&>)
  char *errFile;        // File to redirect stderr or NULL (default)

  struct cmd *left;     // Left subtree or NULL (default)
  struct cmd *right;    // Right subtree or NULL (default)
} CMD;

// Note:  In a [stage] with a HERE document, fromFile should point to a string
// containing the lines in that document.
//
// Note:  In a [stage] with &> (= RED_OUT_ERR) redirection, toType and errType
// should be RED_OUT_ERR, toFile should point to the filename, and errFile
// should be NULL.

									      
// Allocate, initialize, and return a pointer to a command structure of type
// TYPE with left child LEFT and right child RIGHT
CMD *mallocCMD (int type, CMD *left, CMD *right);


// Print the command data structure CMD as a tree whose root is at level LEVEL
void dumpTree (CMD *exec, int level);


// Free the command structure CMD and return NULL
CMD *freeCMD (CMD *cmd);


// Parse a token list into a command structure and return a pointer to
// that structure (NULL if errors found).
CMD *parse (char *line);

#endif
