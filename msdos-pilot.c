/*******************		pilot.c			*****************/

/** This program is an interpreter for the Pilot CAI language as defined 
    in the associated file pilot.bnf.

    		     (C) Copyright 1985, Dave Taylor
**/

#include <stdio.h>		/* Standard C I/O library           */
#include <ctype.h>		/* Character classify functions     */

#define  SLEN			256		/* string length    */
#define  NLEN			20		/* short string     */
#define  COLON			':'		/* colon char       */
#define  STRINGDELIM		'$'		/* delimit stringvar*/
#define  NUMDELIM		'#'		/* delimit num var  */
#ifndef  TRUE
#define  TRUE			1		/* boolean true     */
#define  FALSE			0		/* boolean false    */
#endif
#define  MAXDEPTH		10		/* subroutine depth */
#define  DEFAULT_DEBUG		0		/*   0=OFF, 1=ON    */

#define  NONFATAL		0		/* error 	 */
#define  FATAL			1		/*      type     */

#define  NOERROR		0		/* error func.	 */
#define  ERROR			1		/*    return val */

#define  UNKNOWN_STATEMENT	1		/*  See 	*/
#define  BAD_IDENT		2		/*  errmsg      */
#define  BAD_INSTRUCT		3		/*  below       */
#define  DUP_LABEL		4		/*  for         */
#define  UNKNOWN_LBL		5		/*  a           */
#define  NO_LABEL		6		/*  full        */
#define  UNEXPECT_EOF		7		/*  description */
#define  TOO_DEEP		8		/*  of 		*/
#define  OUT_OF_MEM		9		/*  each	*/
#define  NAME_TOO_LONG		10		/*  error	*/	
#define  UNDEF_VAR		11		/*  code	*/
#define  BAD_NUMBER		12		/*  listed	*/
#define  BAD_PARENS		13		/*  here	*/
#define  BAD_EXP		14
#define  DIVIDE_BY_ZERO		15
#define  BAD_LHS		16
#define  MISSING_EQ		17
#define  STRING_IN_EXP		18
#define  BAD_REL_EXP		19
#define  BAD_REL_OP		20

#define  PLUS			'+'		
#define  MINUS			'-'
#define  TIMES			'*'
#define  DIVIDE			'/'
#define  LEFT_PAREN		'('
#define  RIGHT_PAREN		')'

/****** character classification routines *******/
#define end_of_line(c)		(c == '\n' || c == '\r' || c == '\0')
#define addops(c)		(c == PLUS  || c == MINUS )
#define mulops(c)		(c == TIMES  || c == DIVIDE )
#define mathops(c)		(addops(c) || mulops(c))
#define relop(c) 		(c == '='  || c == '<' || c == '>')
#define paren(c)		(c == LEFT_PAREN  || c == RIGHT_PAREN)
#define special(c)		(mathops(c) || relop(c) || paren(c))
#define stringvar(s)		(s[0] == STRINGDELIM)
#define numvar(s)		(s[0] == NUMDELIM)
#define whitespace(c)		(c == ' ' || c == '\t')
#define valid_char(c)		(isalnum(c) || c == '_')

/****** various one-line routines for the interpreter *******/

#define lastch(s)		s[strlen(s)-1]
#define remove_return(s)	if (lastch(s) == '\n') lastch(s) = '\0' 
#define last_label()		(last->name)
#define init_get_token()	line_loc = 0
#define unget_token()  		line_loc = last_line_loc

char *errmsg[] = { 

/* GENERIC ERROR     */	"Unknown generic error", 
/* UNKNOWN_STATEMENT */ "Unknown statement", 
/* BAD_IDENT         */	"Bad identifier: '%s'",
/* BAD_INSTRUCT      */ "Badly formed instruction", 
/* DUP_LABEL         */ "Duplicate label '%s'",
/* UNKNOWN_LBL	     */	"Unknown label '%s'", 
/* NO_LABEL          */ "Label '%s' not found in program",
/* UNEXPECTED_EOF    */	"End of File during search for label '%s'",
/* TOO_DEEP	     */ "Routine calls nested too deeply",
/* OUT_OF_MEM	     */ "Out of memory!", 
/* NAME_TOO_LONG     */ "Name '%s' too long",
/* UNDEF_VAR         */ "Undefined variable: '%s'",
/* BAD_NUMBER        */ "Invalid format for numerical input!", 
/* BAD_PARENS	     */ "Badly formed expression: parenthesis",
/* BAD_EXP           */ "Badly formed expression",
/* DIVIDE_BY_ZERO    */ "Attempt to divide by zero!",
/* BAD_LHS	     */ "Bad left-hand-side of expression",
/* MISSING_EQ	     */ "Missing or misplaced '=' in expression",
/* STRING_IN_EXP     */ "String variable in numerical expression",
/* BAD_REL_EXP       */ "Bad relational expression",
/* BAD_REL_OP	     */ "Bad relational operator: '%s'" };

struct a_label {		/* The label table is a linked list */
	 char	name[NLEN];	/*   of label name,                 */
	 int    loc;		/*   the line number it occurs on,  */
	 struct a_label *next;	/*   and a link to the next element */
       } *label_list, *last;

struct symbol_entry {		/* The symbol table is a binary tree */
	 char   name[NLEN];	/*   of symbol name                  */
	 char   value[SLEN];	/*   the printable current value     */
	 int    numvalue;	/*   the numeric value if number var */
	 struct symbol_entry
	       *left,		/*   a left subnode link             */
	       *right;		/*   and a right subnode link        */
       } *symbol_table, *symbol_node;

int    subroutine_stack[MAXDEPTH];   /* subroutine calls and returns */

FILE *fileid;			/* input file descriptor      */
char  def_string[SLEN];		/* line read from stdin       */
int   current_line = 0, 	/* line being read from file  */
      line_loc, 		/* for line -> words transl.  */
      last_line_loc,		/* for unget_token() routine  */
      boolean=TRUE,		/* result of last match       */
      boolean_cont=FALSE,	/* result of last bool test   */
      nesting_level = 0,	/* how deep into 'use' calls  */
      error,			/* error during exp parsing   */
      furthest_into_file = 0,	/* furthest line read in file */
      debug=DEFAULT_DEBUG;	/* is debugging turned on?    */

char *get_token();		/* forward declaration        */

main(argc, argv)
int argc;
char *argv[];
{
	char line[SLEN];	/* input buffer for reading file */

	if (argc == 3)
	  debug++;
	else if (argc != 2)
	  exit(printf("Usage: %s [-d] <filename>\n", argv[0]));

	init(argv[arc-1]);		  /* open file and such */

	while (fgets(line, SLEN, fileid) != NULL) {
	  remove_return(line);	
	  current_line++;
	  if (debug) printf(" %2d > %s\n", current_line, line);
	  if (strlen(line) > 0) parse(line);
	}

	wrapup();			 /* close file and such */
}

init(fname)
char *fname;
{
	/** initialize the interpreter - open file and related. **/
	
	if ((fileid = fopen(fname, "r")) == NULL)
	  wrapup(printf("** Fatal error: Could not open '%s'!\n", fname));

	label_list = NULL;
}

wrapup()
{
	/** End of interpreter session - close file & list if debug **/

	if (debug) {
	  printf("\nDump of symbol table:\n");
	  print_symbol_table(symbol_table);
	  printf("\nDump of label table:\n");
	  last = label_list;
	  while (last != NULL) {
	    printf("%-20.20s at line %d\n", last->name, last->loc);
	    last = last->next;
	  }
	}

	fclose(fileid);
	exit(0);
}

parse(line)
char *line;
{
	/** Parse line, calling the appropriate routine depending on
	    the statement type. **/

	register int i=0;
	char *lineptr, buffer[SLEN];

	while (whitespace(line[i])) 	i++;	   /* skip leading blanks */

	lineptr = (char *) line + i+1;		   /* ...skip first char  */

	error = NOERROR;			   /* each line is new!   */

	switch (toupper(line[i])) {
	  case 'A' : accept(lineptr);			break;
	  case 'C' : compute(lineptr);			break;
	  case 'E' : endit(lineptr);			break;
	  case 'J' : jump(lineptr);			break;
	  case 'M' : match(lineptr);			break;
	  case 'R' : /** remark **/     		break;
	  case 'T' : type(lineptr);			break;
	  case 'U' : use(lineptr);			break;

	  case '*' : label(line, current_line);	break;
	  case ':' : sprintf(buffer, "%c%s", boolean_cont==boolean? 'Y' : 'N', 
			     line);
		     type(buffer);	          	break;	
	  default  : 
		     raise_error(UNKNOWN_STATEMENT, NONFATAL, line);
	}
}

type(line)
char *line;
{
	/** this routine outputs the given line to the screen.  **/

     	if (! (boolean_cont = check_condition(line))) return;

	if (substitute_vars(line) != ERROR)
	  printf("%s\n", line);
}

accept(line)
char *line;
{
	/** This routine accepts a line of input and sets def_string
	    to the value.  If a variable is specified, it uses that. **/

	struct symbol_entry *entry, *add_symbol();
	char  *name;
	int   defined_symbol = 0, value_buffer, i;

	if (! (boolean_cont = check_condition(line))) return;
	
	init_get_token(); 

	if (strlen(line) > 0) 		 /* variable name to use! */
  	  if ((name = get_token(line)) != NULL) {
	    if (! stringvar(name) && ! numvar(name))
	       return(raise_error(BAD_IDENT, NONFATAL, name));
	    symbol_table = add_symbol(symbol_table, name);
	    entry = symbol_node;	/* set to new node address */
	    defined_symbol++;
	  }

	printf("> ");

	gets(def_string, SLEN);
	remove_return(def_string);	/* remove return, if any */
	
	if (numvar(name)) {		/* special processing for number */
	  i = 0;
	  if (def_string[i] == '-') i++;
	  for (; i < strlen(def_string); i++)
	    if (! isdigit(def_string[i]))
	      wrapup(raise_error(BAD_NUMBER, FATAL, NULL));

	  if (defined_symbol) {			/* do we need save this?  */
	    sscanf(def_string,"%d", &value_buffer);
	    entry->numvalue = value_buffer;     /* keep as a number and   */
	    strcpy(entry->value, def_string);	/*       as string too!   */
	  }
	}	
	else if (defined_symbol)	        /* do we need to save it? */
	  strcpy(entry->value, def_string);	/*   then named = default */
}
	     
match(line)
char *line;
{
	/** This will try to match any of the list of words or string 
	    variables (delimited by spaces) in the list to the value 
	    of def_string, the last line input.  Variables are expanded 
	    to their values.  Possible errors: UNDEF_VAR     	    **/

	struct symbol_entry *entry, *find_symbol();
	char *word; 

	if (! (boolean_cont = check_condition(line))) return;
	
	init_get_token();

	while ((word = get_token(line)) != NULL) {
	  if (stringvar(word) || numvar(word)) {
	    if ((entry = find_symbol(symbol_table, word)) == NULL)
	      return(raise_error(UNDEF_VAR, NONFATAL, word));
	    else
	      strcpy(word, entry->value);     /* word = value of var */
	  } 
	  if (boolean = in_string(def_string, word))
	    return; 	/* done as soon as hit a match... */
	}
}

jump(line)
char *line;
{
	/** This will jump to the indicated label, if possible **/

	if (! (boolean_cont = check_condition(line))) return;
	
	get_to_label(find_label((char *) line + 1), (char *) line + 1);
}

endit(line)
char *line;
{
	/** This marks the end of a routine or of the program **/

	if (! (boolean_cont = check_condition(line))) return;
	
	if (nesting_level == 0)
	  wrapup();	/* done with entire program! */
	else 
	  get_to_label(subroutine_stack[--nesting_level], NULL);
}

use(line)
char *line;
{
	/** Call the specified subroutine.  Possible errors: TOO_DEEP **/

	if (! (boolean_cont = check_condition(line))) return;
	
	line = (char *) line + 1;

	if (nesting_level == MAXDEPTH)
	  wrapup(raise_error(TOO_DEEP, FATAL, NULL));

	subroutine_stack[nesting_level++] = current_line;

	get_to_label(find_label(line), line);
}

compute(line)
char *line;
{
	/** Compute the indicated expression based on whether it's a 
	    numerical or string expression.  
	    Possible errors: BAD_LHS, MISSING_EQ **/

	struct symbol_entry *node;
	char   *ident, buffer[SLEN];
	int    value = 0, i=0, j=0;

	if (! (boolean_cont = check_condition(line))) return;

	init_get_token();

	if ((ident = get_token(line)) == NULL)
	  return(raise_error(BAD_LHS, NONFATAL, NULL));

	if (!stringvar(ident) && !numvar(ident))
	  return(raise_error(BAD_LHS, NONFATAL, NULL));

	symbol_table = add_symbol(symbol_table, ident);
	
	node = symbol_node;	/* keep structure to save to.. */

	if ((ident = get_token(line)) == NULL)
	  return(raise_error(MISSING_EQ, NONFATAL, NULL));

	if (ident[0] != '=')
	  return(raise_error(MISSING_EQ, NONFATAL, NULL));

	if (stringvar(node->name)) {	/* string expression */
	  /* Get 'rest' of line for string substitution      */
	  for (i=line_loc; ! end_of_line(line[i]); i++)
	    buffer[j++] = line[i]++;
	  buffer[j] = 0;
	  if (substitute_vars(buffer) == ERROR) return;
	  strcpy(node->value, buffer);
	}
	else {				/* numerical expression */
	  value = evaluate(line);	

	  if (! error) {
	    node->numvalue = value;
	    sprintf(node->value, "%d", value);
	  }
	}
}

label(line, loc)
char *line;
int  loc;
{
	/** add label to label table at line number loc, but only if
	    we haven't read this part of program before! **/

	if (loc > furthest_into_file) {
	  add_label(line);
	  furthest_into_file = loc;
	}
}

int
raise_error(errno, errtype, arg)
int errno, errtype;
char *arg;
{
	/** Display error 'errno', type FATAL or NONFATAL, arg, if present,
	    is output too. **/

	char buffer[SLEN];

	if (errtype == FATAL) 
	  sprintf(buffer, "** FATAL Error: %s\n", errmsg[errno]);
	else
	  sprintf(buffer, "** Error: %s on line %d\n", 
		  errmsg[errno], current_line);

	printf(buffer, arg);

	return(ERROR);
}

int 
in_string(buffer, pattern)
char *buffer, *pattern;
{
	/** Returns TRUE iff pattern occurs IN IT'S ENTIRETY in buffer. **/ 

	register int i = 0, j = 0;
	
	while (buffer[i] != '\0') {
	  while (buffer[i++] == pattern[j++]) 
	    if (pattern[j] == '\0') 
	      return(TRUE);
	  i = i - j + 1;
	  j = 0;
	}
	return(FALSE);
}

int
check_condition(line)
char *line;
{
	/** This routine returns non-zero iff the indicated condition (if 
	    any) is TRUE.  This routine will also remove the part of the 
	    line that contains the actual conditional and the colon.  **/

	char     buffer[SLEN];

	strcpy(buffer, line);		/* save the line.. */

	if (! remove_past_colon(line))	
	  return(ERROR);

	if (buffer[0] == '(') 		/* relational expression! */
	  return(relation(buffer));
	else if (buffer[0] == 'Y') 	/* if boolean... */
	  return(boolean);
	else if (buffer[0] == 'N') 	/* if ! boolean  */
	  return(! boolean);
	else 				/* always...     */
	  return(TRUE);
}

int
remove_past_colon(line)
char *line;
{
	/** Remove up to and including the 'colon' from input string 
	    Returns zero iff no colon - Possible error: BAD_INSTRUCT 
	**/

	register int i = 0, index;

	while (line[i] != COLON && line[i] != '\0') 	i++;

	if (line[i] == COLON) 
	  i++;	/* get past colon */
	else	/* no colon in line!  Bad construct!  */
	  return(raise_error(BAD_INSTRUCT, NONFATAL, line));

	for (index = i; line[index] != '\0'; index++)
	  line[index-i] = line[index];

	line[index-i] = '\0';
	
	return(TRUE);
}

int
break_string(line, lhs, op, rhs)
char *line, *lhs, *op, *rhs;
{
	/** Breaks down line into left-hand-side, relational operator and 
	    right-hand-side.  We need to strip out parens surrounding the
	    expression too without saving them.
	    Possible errors: BAD_REL_OP, BAD_REL_EXP **/
	
	char *word;

	lhs[0] = rhs[0] = op[0] = '\0';	/* initialize them all */

	init_get_token();

	if (get_token(line) == NULL)	/* no open parenthesis! */
	  return(raise_error(BAD_REL_EXP, NONFATAL, NULL));

	/** get lhs ... **/

	while ((word = get_token(line)) != NULL && ! relop(word[0]) && 
		word[0] != COLON && ! paren(word[0])) 
	  sprintf(lhs, "%s%s", lhs, word);

	if (word == NULL)
	  return(raise_error(BAD_REL_EXP, NONFATAL, NULL));

	if (paren(word[0]))  	/* nonexpression relational  */
	  return(NOERROR);	

	/** get op ... **/

	strcpy(op, word);

	/** get rhs ... **/

	while ((word = get_token(line)) != NULL && word[0] != COLON) 
	  sprintf(rhs, "%s%s", rhs, word);

	lastch(rhs) = '\0';	/* remove last closing paren */
	
	if (word == NULL)
	  return(raise_error(BAD_REL_EXP, NONFATAL, NULL));
	else
	  return(NOERROR);
}

add_label(name)
char *name;
{
	/** Add given label to label list at end, if not found first.  
	    Possible errors: DUP_LABEL, OUT_OF_MEM 	 	  **/

	struct a_label *previous;

	previous = label_list;	/* both previous and 	   */
	last     = label_list;	/*   last are set to head */

	while (last != NULL) {
	  if (strcmp(last->name, name) == 0)
	    return(raise_error(DUP_LABEL, NONFATAL, name));
	  previous = last;
	  last = last->next;
	}

	/** at this point entry == NULL and previous == last valid entry **/

	if ((last = (struct a_label *) malloc(sizeof *last)) == NULL)
	  wrapup(raise_error(OUT_OF_MEM, FATAL, NULL));	 /* no memory! */

	strncpy(last->name, name, NLEN);
	last->loc = current_line;
	last->next= NULL;

	if (previous == NULL)
	  label_list = last;	/* first element in list */
	else
	  previous->next = last;
}
	
int
find_label(name)
char *name;
{
	/** Returns line location of specified label or 0 if that label 
	    isn't currently in the label table. **/

	struct a_label *entry;

	entry = label_list;	/* set entry to label list head */

	while (entry != NULL) {
	  if (strcmp(entry->name, name) == 0)
	    return(entry->loc);
	  entry = entry->next;
	}

	return(0);
}

int
get_to_label(loc, labelname)
int loc;
char *labelname;
{
	/** Move file pointer to indicated line number.  If loc is zero this 
	    indicates that we need to scan FORWARD in the file, so reads 
	    quickly forward, adding newly encountered labels to the label 
	    list as encounted.  If loc is non-zero, get to specified line 
	    from current line in minimal movement possible.
	    Possible errors: NO_LABEL, UNEXPECT_EOF
	**/
	
	char buffer[SLEN];

	if (loc == 0) { 		/** forward scan... **/

	  if (debug) printf("\tget_to_label(%s)\n", labelname);
	  while (fgets(buffer, SLEN, fileid) != NULL) {
	    remove_return(buffer);
	    current_line++;
	    if (debug) printf("%d >> %s\n", current_line, buffer);
	    if (buffer[0] == '*') {
	      if (label(buffer, current_line) == ERROR)
		wrapup(raise_error(UNEXPECT_EOF, FATAL, labelname));
	      if (strcmp(labelname, last_label()) == 0)
	        return;	 /* label found! */
	    } 
	  }

	  wrapup(raise_error(NO_LABEL, FATAL, labelname));
	}
	else {	  		      /* get to specified line... */
	  if (loc < current_line) {   /*   if before, rewind file */
	    rewind(fileid);
	    current_line = 0;
	  }

	  while (fgets(buffer, SLEN, fileid) != NULL) {
	    current_line++;
	    if (current_line == loc)
	      return;
	  } 
	  wrapup(raise_error(UNEXPECT_EOF, FATAL, NULL));
	}
}

struct symbol_entry *add_symbol(node, symbol)
struct symbol_entry *node;
char *symbol;
{
	/** This routine adds the specified symbol to the symbol table.  
	    The first character determines the type of the variable: '$' for 
	    strings and '#' for integers.  Possible errors: OUT_OF_MEM **/
	
	int    cond;

	if (node == NULL) {
	  if ((node = (struct symbol_entry *) malloc(sizeof *node)) == NULL)
	    wrapup(raise_error(OUT_OF_MEM, FATAL, NULL));
	  strcpy(node->name, symbol);
	  node->value[0] = '\0';
	  node->numvalue = 0;
	  node->left  = NULL;
	  node->right = NULL;
	  symbol_node = node;	/* store address globally too, if needed! */
	}
	else if ((cond = strcmp(symbol, node->name)) == 0)
	  symbol_node = node;	/* store address globally too, if needed! */
	else if (cond < 0)
	  node->left = add_symbol(node->left, symbol);
	else
	  node->right= add_symbol(node->right, symbol);
	
	return(node);
}
	
print_symbol_table(node)
struct symbol_entry *node;
{
	/** Recursively lists all entries in the symbol table.  Debug only **/

	if (node != NULL) {
	  print_symbol_table(node->left);
	  printf("\t%-20.20s '%s'\n", node->name, node->value);	
	  print_symbol_table(node->right);
	}
}

struct symbol_entry *find_symbol(node, symbol)
struct symbol_entry *node;
char *symbol;
{
	/** Returns either NULL if the symbol is not found or the address of 
	    the structure containing the specified symbol.  This is a stan-
	    dard recursive binary tree search...			 **/

	int    cond;

	if (node == NULL) 
	  return(NULL);
	else if ((cond = strcmp(symbol, node->name)) == 0)
	  return(node);	/* store if needed */
	else if (cond < 0) 
	  return(find_symbol(node->left, symbol));
	else 
	  return(find_symbol(node->right, symbol));
}

int
substitute_vars(line)
char *line;
{
	/** This routine substitutes the value for each variable it finds in 
	    the given line.  Possible error: UNDEF_VAR 			 **/

	struct symbol_entry *entry, *find_symbol();
	register int i = 0, j = 0, word_index;
	char word[NLEN], buffer[SLEN];

	do {

	  /** while not in variable copy to buffer... **/

	  while (line[i] != STRINGDELIM && line[i] != NUMDELIM && 
	         ! end_of_line(line[i]))
	    buffer[j++] = line[i++];

	  /** get variable if it exists... **/

	  word_index = 0;

	  if (! end_of_line(line[i]))
	    word[word_index++] = line[i++];	/* copy in the delimiter */
  
	  while (! end_of_line(line[i]) && valid_char(line[i]))
	    word[word_index++] = line[i++];
  
	  /* Have a variable?  if so try to find and substitute */
  
	  if (word_index > 0) {
	    word[word_index] = '\0';
	    if ((entry = find_symbol(symbol_table, word)) == NULL)
	      return(raise_error(UNDEF_VAR, NONFATAL, word));
  
	    for (word_index=0; word_index < strlen(entry->value); word_index++)
	      buffer[j++] = (entry->value)[word_index];
	  }

	} while (! end_of_line(line[i]));
	
	buffer[j] = '\0';
	strcpy(line, buffer);	/* copy it back in... */
	return(NOERROR);
}

int
relation(exp)
char *exp;
{
	/** Evaluate relational expression between a set of parenthesis.  
	    Returns TRUE or FALSE according to the results of the evaluation.  
	    If an error occurs this routine will always return FALSE.
	    Possible errors: BAD_REL_OP                                   **/

	char  word[NLEN], lhs_string[SLEN], rhs_string[SLEN];
	int   retval, lhs, rhs;	/* left hand & right hand side */

	if (break_string(exp, lhs_string, word, rhs_string) == ERROR)
	  return(FALSE);	/* default for error */

	init_get_token();			/* new string:    */
	lhs = evaluate(lhs_string);		/* left hand side */
	
	if (error) return(FALSE);	/* erroneous always fail  */

	if (word[0] == '\0' && rhs_string[0] == '\0')
	  return(lhs != 0); 		/* accept no relation exp. */

	init_get_token();			/* new string:     */
	rhs = evaluate(rhs_string);		/* right hand side */

	if (error) return(FALSE);	/* erroneous always fail   */	

	switch (word[0]) {		/* compute return value   */
	  case '=' : retval = (lhs == rhs);		break;
	  case '<' : switch (word[1]) {
			case '\0' : retval = (lhs < rhs);	break;
		        case '>'  : retval = (lhs != rhs);	break;
	                case '='  : retval = (lhs <= rhs);	break;
  			default   : return(raise_error(BAD_REL_OP, NONFATAL,
					   word));
	             }
	             break;
	  case '>' : switch (word[1]) {
			case '\0' : retval = (lhs > rhs);	break;
	                case '='  : retval = (lhs >= rhs);	break;
			default   : return(raise_error(BAD_REL_OP, NONFATAL,
					   word));
	             }
		     break;
  	  default  : return(raise_error(BAD_REL_OP, NONFATAL, word));
	}

	return(retval);
}	

int
evaluate(exp)
char *exp;
{
	/** Evaluate expression and check that we've read all the tokens 
	    Returns value or ERROR.      Possible errors: BAD_PARENS **/

	int value;

	value = expression(exp);

	if (get_token(exp) != NULL) 
	  return((! error++) ? 
	    raise_error(BAD_PARENS, NONFATAL, NULL) : 0);
	else
	  return(value);
}

int
expression(exp)
char *exp;
{
	/** Recursively evaluate the expression given. **/

	char *word;
	int val = 0;

	val = term(exp);	

	if ((word = get_token(exp)) == NULL) 
	  return(val);

	if (! addops(word[0])) {
	  unget_token();
	  return(val);
	}

	while (addops(word[0])) {
	  if (word[0] == PLUS)
	     val += term(exp);
	  else 	/* must be MINUS */
	     val -= term(exp);

	  if ((word = get_token(exp)) == NULL) 
	    return(val);
	  if (! addops(word[0])) {
	    unget_token();
	    return(val);
	  }
	}

	return(val);
}

term(exp)
char *exp;
{
	/** Get a term (ie a multiply or divide) and return the results 
	    of computing it.  Possible errors: DIVIDE_BY_ZERO       **/

	register int val = 0, value;
	char *word;

	val = factor(exp);

	if ((word = get_token(exp,3)) == NULL) 
	  return(val);

	if (! mulops(word[0])) {
	  unget_token();
	  return(val);
	}
	
	while (mulops(word[0])) {

	  if (word[0] == TIMES)
	    val *= factor(exp);
	  else
	    if ((value = factor(exp)) == 0) { 
	      if (! error++)
	        wrapup(raise_error(DIVIDE_BY_ZERO, FATAL, NULL));
	      else
	        return(0);
	    }
	    else
	      val /= value;

	  if ((word = get_token(exp)) == NULL) 
	    return(val);

	  if (! mulops(word[0])) {
	    unget_token();
	    return(val);
	  }
	}

	return(val);
}

factor(string)
char *string;
{
	/** Break down a string - either another expression in parentheses,
	    a specific numerical value or a variable name 
	    Possible errors: BAD_EXP, BAD_PARENS, STRING_IN_EXP, UNDEF_VAR
	**/
	
	struct symbol_entry *entry, *find_symbol();
	int  val = 0;
	char *word;

	if ((word = get_token(string)) == NULL) 
	  return((! error++)? raise_error(BAD_EXP, NONFATAL, NULL) : 0);

	if (word[0] == LEFT_PAREN) {
	  val = expression(string);
	  if ((word = get_token(string)) == NULL) 
	    return((! error++)? 
	      raise_error(BAD_PARENS, NONFATAL, NULL):0);
	  else if (word[0] != RIGHT_PAREN) 
	    return((! error++)? 
	      raise_error(BAD_EXP, NONFATAL, NULL):0);
	}
	else if (stringvar(word))	/* what's THIS doing here?? */
	  return((! error++)?
	    raise_error(STRING_IN_EXP, NONFATAL, NULL):0);
	else if (numvar(word)) {
	  if ((entry = find_symbol(symbol_table, word)) == NULL)
	    return((! error++)?
	      raise_error(UNDEF_VAR, NONFATAL, word):0);
	  val = entry->numvalue;
	}
	else if (word[0] == '-') {	/* minus number */
	  if ((word = get_token(string)) == NULL) 
	    return((! error++)? raise_error(BAD_EXP, NONFATAL, NULL) : 0);
	  val = -atoi(word);
	}
	else
	  val = atoi(word);
	
	return(val);
}


char *get_token(line)
char *line;
{
	/** Return the next token in the line without surrounding white spaces.
	    Return zero if at end-of-line **/

	static char word[SLEN];
	register int i = 0;

	while (whitespace(line[line_loc]))
	  line_loc++;

	last_line_loc = line_loc;

	if (end_of_line(line[line_loc])) 
	  return(NULL);

	if (mathops(line[line_loc]) || paren(line[line_loc])) 
	  word[i++] = line[line_loc++];
	else if (relop(line[line_loc])) {
	  word[i++] = line[line_loc++];
	  if (relop(line[line_loc]) && line[line_loc] != line[line_loc-1])
	    word[i++] = line[line_loc++];
	}
	else {
	  while (! special(line[line_loc]) && ! whitespace(line[line_loc]) &&
	         ! end_of_line(line[line_loc]))
	    if (i == NLEN-1) {
	      word[i] = '\0';
	      wrapup(raise_error(NAME_TOO_LONG, FATAL, word));
	    }
	    else
	      word[i++] = line[line_loc++];
	}

	word[i] = '\0';

	return( (char *) word);
}
