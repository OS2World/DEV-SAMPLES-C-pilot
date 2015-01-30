/* -------
 * pilot.c -- Pilot CAI interpreter (C) Dave Taylor.
 * -------
 * This program is an interpreter for the Pilot CAI language as defined
 * in the associated file pilot.bnf.
 *
 * Original program (C) Copyright 1985, Dave Taylor.
 * OS/2 port, Tommi Nieminen 18-Aug-1993.
 *
 * Notes on OS/2 port:
 * Some modification was needed to get this compiled with gcc 2.4.5 and
 * emx 0.8g. Also, "EXTPROC" support was added to implement Pilot CAI
 * as an external command processor (see `read.me' file for further
 * information on this).
 *
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

  /* Program name to be used in messages */
#define  PRGNAME        "Pilot CAI for OS/2 v1.0"

#define  SLEN           256     /* string length    */
#define  NLEN           20      /* short string     */
#define  COLON          ':'     /* colon char       */
#define  STRINGDELIM    '$'     /* delimit stringvar*/
#define  NUMDELIM       '#'     /* delimit num var  */
#ifndef  TRUE
#define  TRUE           1       /* boolean true     */
#define  FALSE          0       /* boolean false    */
#endif
#define  MAXDEPTH       10      /* subroutine depth */
#define  DEFAULT_DEBUG  0       /* 0=OFF, 1=ON      */

  /* Error types */
#define  NONFATAL       0
#define  FATAL          1

  /* Return codes */
#define  NOERROR        0
#define  ERROR          1

  /* Error codes */
#define  UNKNOWN_STATEMENT   1
#define  BAD_IDENT           2
#define  BAD_INSTRUCT        3
#define  DUP_LABEL           4
#define  UNKNOWN_LBL         5
#define  NO_LABEL            6
#define  UNEXPECT_EOF        7
#define  TOO_DEEP            8
#define  OUT_OF_MEM          9
#define  NAME_TOO_LONG      10
#define  UNDEF_VAR          11
#define  BAD_NUMBER         12
#define  BAD_PARENS         13
#define  BAD_EXP            14
#define  DIVIDE_BY_ZERO     15
#define  BAD_LHS            16
#define  MISSING_EQ         17
#define  STRING_IN_EXP      18
#define  BAD_REL_EXP        19
#define  BAD_REL_OP         20

#define  PLUS           '+'
#define  MINUS          '-'
#define  TIMES          '*'
#define  DIVIDE         '/'
#define  LEFT_PAREN     '('
#define  RIGHT_PAREN    ')'

  /* Character classification routines */
#define end_of_line(c)  (c == '\n' || c == '\r' || c == '\0')
#define addops(c)       (c == PLUS  || c == MINUS )
#define mulops(c)       (c == TIMES  || c == DIVIDE )
#define mathops(c)      (addops(c) || mulops(c))
#define relop(c)        (c == '='  || c == '<' || c == '>')
#define paren(c)        (c == LEFT_PAREN  || c == RIGHT_PAREN)
#define special(c)      (mathops(c) || relop(c) || paren(c))
#define stringvar(s)    (s[0] == STRINGDELIM)
#define numvar(s)       (s[0] == NUMDELIM)
#define whitespace(c)   (c == ' ' || c == '\t')
#define valid_char(c)   (isalnum(c) || c == '_')

  /* Various one-line routines for the interpreter */
#define lastch(s)           s[strlen(s) - 1]
#define remove_return(s)    if (lastch(s) == '\n') lastch(s) = '\0'
#define last_label()        (last->name)
#define init_get_token()    line_loc = 0
#define unget_token()       line_loc = last_line_loc

char *errmsg[] = {
    "Unknown generic error",                       /* GENERIC ERROR     */
    "Unknown statement",                           /* UNKNOWN_STATEMENT */
    "Bad identifier: \"%s\"",                      /* BAD_IDENT         */
    "Badly formed instruction",                    /* BAD_INSTRUCT      */
    "Duplicate label \"%s\"",                      /* DUP_LABEL         */
    "Unknown label \"%s\"",                        /* UNKNOWN_LBL       */
    "Label \"%s\" not found in program",           /* NO_LABEL          */
    "End of File during search for label \"%s\"",  /* UNEXPECTED_EOF    */
    "Routine calls nested too deeply",             /* TOO_DEEP          */
    "Out of memory!",                              /* OUT_OF_MEM        */
    "Name \"%s\" too long",                        /* NAME_TOO_LONG     */
    "Undefined variable: \"%s\"",                  /* UNDEF_VAR         */
    "Invalid format for numerical input!",         /* BAD_NUMBER        */
    "Badly formed expression: parenthesis",        /* BAD_PARENS        */
    "Badly formed expression",                     /* BAD_EXP           */
    "Attempt to divide by zero!",                  /* DIVIDE_BY_ZERO    */
    "Bad left-hand-side of expression",            /* BAD_LHS           */
    "Missing or misplaced \"=\" in expression",    /* MISSING_EQ        */
    "String variable in numerical expression",     /* STRING_IN_EXP     */
    "Bad relational expression",                   /* BAD_REL_EXP       */
    "Bad relational operator: \"%s\""              /* BAD_REL_OP        */
};

struct a_label {            /* The label table is a linked list */
    char name[NLEN];        /*   of label name,                 */
    int loc;                /*   the line number it occurs on,  */
    struct a_label *next;   /*   and a link to the next element */
} *label_list, *last;

struct symbol_entry {       /* The symbol table is a binary tree */
    char name[NLEN];        /*   of symbol name                  */
    char value[SLEN];       /*   the printable current value     */
    int numvalue;           /*   the numeric value if number var */
    struct symbol_entry
        *left,              /*   a left subnode link             */
        *right;             /*   and a right subnode link        */
} *symbol_table, *symbol_node;

  /* Subroutine calls and returns */
int subroutine_stack[MAXDEPTH];

FILE *fileid;                 /* input file descriptor      */
char def_string[SLEN];        /* line read from stdin       */
int current_line = 0,         /* line being read from file  */
    line_loc,                 /* for line -> words transl.  */
    last_line_loc,            /* for unget_token() routine  */
    boolean = TRUE,           /* result of last match       */
    boolean_cont = FALSE,     /* result of last bool test   */
    nesting_level = 0,        /* how deep into 'use' calls  */
    error,                    /* error during exp parsing   */
    furthest_into_file = 0,   /* furthest line read in file */
    debug = DEFAULT_DEBUG;    /* is debugging turned on?    */

  /* Function declarations */
void init(char *fname);
void wrapup(void);
void parse(char *line);
void type(char *line);
void accept(char *line);
void match(char *line);
void jump(char *line);
void endit(char *line);
void use(char *line);
void compute(char *line);
int label(char *line, int loc);
int raise_error(int errno, int errtype, char *arg);
int in_string(char *buffer, char *pattern);
int check_condition(char *line);
int remove_past_colon(char *line);
int break_string(char *line, char *lhs, char *op, char *rhs);
void add_label(char *name);
int find_label(char *name);
int get_to_label(int loc, char *labelname);
struct symbol_entry *add_symbol(struct symbol_entry *node, char *symbol);
void print_symbol_table(struct symbol_entry *node);
struct symbol_entry *find_symbol(struct symbol_entry *node, char *symbol);
int substitute_vars(char *line);
int relation(char *exp);
int evaluate(char *exp);
int expression(char *exp);
int term(char *exp);
int factor(char *string);
char *get_token(char *line);

main(int argc, char *argv [])
{
    char line[SLEN];  /* Input buffer for reading file */

    if (argc == 3)
        debug ++;
    else if (argc != 2) {
        printf("%s (C) Dave Taylor 1985.\n", PRGNAME);
        printf("OS/2 port--Tommi Nieminen 18-Aug-1993.\n");
        printf("\n\tUsage: [D:\\] pilot [ -d ] FILE\n");
        printf("\n-d switch turns on debugging mode.\n");
        exit(1);
    }

      /* Open file and such */
    init(argv[argc - 1]);

    while (fgets(line, SLEN, fileid) != NULL) {
        remove_return(line);

          /* TN 18-Aug-1993:
           * Ignore EXTPROC (note: this ignores EXTPROC anywhere,
           * not just at the beginning of file)
           */
        if (strnicmp(line, "EXTPROC", 7) == 0)
            *line = '\0';

        current_line ++;
        if (debug)
            printf(" %2d > %s\n", current_line, line);
        if (strlen(line) > 0)
            parse(line);
    }

      /* Close file and such */
    wrapup();
}

void init(char *fname)
  /* Initialize the interpreter - open file and related. */
{
    if ((fileid = fopen(fname, "r")) == NULL) {
        fprintf(stderr, "Fatal error: Could not open \"%s\"\n", fname);
        wrapup();
    }

    label_list = NULL;
}

void wrapup(void)
  /* End of interpreter session - close file & list if debug */
{
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

void parse(char *line)
  /* Parse line, calling the appropriate routine depending on
   * the statement type.
   */
{
    register int i = 0;
    char *lineptr, buffer[SLEN];

      /* Skip leading blanks */
    while (whitespace(line[i]))
        i ++;

      /* ...skip first char */
    lineptr = (char *) line + i + 1;

      /* Each line is new! */
    error = NOERROR;

    switch (toupper(line[i])) {
        case 'A' :  accept(lineptr);
                    break;
        case 'C' :  compute(lineptr);
                    break;
        case 'E' :  endit(lineptr);
                    break;
        case 'J' :  jump(lineptr);
                    break;
        case 'M' :  match(lineptr);
                    break;
        case 'R' :  /* Remark */
                    break;
        case 'T' :  type(lineptr);
                    break;
        case 'U' :  use(lineptr);
                    break;
        case '*' :  label(line, current_line);
                    break;
        case ':' :  sprintf(buffer, "%c%s", boolean_cont==boolean? 'Y' : 'N',
                      line);
                    type(buffer);
                    break;
        default  :  raise_error(UNKNOWN_STATEMENT, NONFATAL, line);
    }
}

void type(char *line)
  /* Outputs the given line to the screen. */
{
    if (!(boolean_cont = check_condition(line)))
        return;

    if (substitute_vars(line) != ERROR)
        printf("%s\n", line);
}

void accept(char *line)
  /* This routine accepts a line of input and sets def_string
   * to the value.  If a variable is specified, it uses that.
   */
{
    struct symbol_entry *entry, *add_symbol();
    char *name;
    int defined_symbol = 0, value_buffer, i;

    if (!(boolean_cont = check_condition(line)))
        return;

    init_get_token();

    if (strlen(line) > 0)  /* Variable name to use! */
        if ((name = get_token(line)) != NULL) {
            if (!stringvar(name) && !numvar(name)) {
                raise_error(BAD_IDENT, NONFATAL, name);
                return;
            }
            symbol_table = add_symbol(symbol_table, name);
            entry = symbol_node; /* Set to new node address */
            defined_symbol ++;
        }

    printf("> ");

    fgets(def_string, SLEN, stdin);
    remove_return(def_string);  /* Remove return, if any */

    if (numvar(name)) {  /* Special processing for number */
        i = 0;
        if (def_string[i] == '-')
          i ++;
        for (; i < strlen(def_string); i ++)
            if (!isdigit(def_string[i])) {
                raise_error(BAD_NUMBER, FATAL, NULL);
                wrapup();
            }

        if (defined_symbol) {  /* Do we need save this?  */
            sscanf(def_string,"%d", &value_buffer);
            entry->numvalue = value_buffer;    /* Keep as a number and */
            strcpy(entry->value, def_string);  /* as string too!       */
        }
    }
    else if (defined_symbol)  /* Do we need to save it? */
        strcpy(entry->value, def_string);  /* Then named = default */
}

void match(char *line)
  /* Try to match any of the list of words or string  variables
   * (delimited by spaces) in the list to the value of def_string,
   * the last line input. Variables are expanded to their values.
   *   Possible errors: UNDEF_VAR
   */
{
    struct symbol_entry *entry;
    char *word;

    if (!(boolean_cont = check_condition(line)))
        return;

    init_get_token();

    while ((word = get_token(line)) != NULL) {
        if (stringvar(word) || numvar(word)) {
            if ((entry = find_symbol(symbol_table, word)) == NULL) {
                raise_error(UNDEF_VAR, NONFATAL, word);
                return;
            }
            else
                strcpy(word, entry->value);  /* Word = value of var */
        }
        if (boolean = in_string(def_string, word))
            return; /* Done as soon as hit a match... */
    }
}

void jump(char *line)
  /* Jump to the indicated label, if possible */
{
    if (!(boolean_cont = check_condition(line)))
        return;
    get_to_label(find_label((char *) line + 1), (char *) line + 1);
}

void endit(char *line)
  /* This marks the end of a routine or of the program */
{
    if (!(boolean_cont = check_condition(line)))
        return;

    if (nesting_level == 0)
        wrapup();  /* Done with entire program! */
    else
        get_to_label(subroutine_stack[-- nesting_level], NULL);
}

void use(char *line)
  /* Call the specified subroutine.
   *   Possible errors: TOO_DEEP
   */
{
    if (!(boolean_cont = check_condition(line)))
        return;

    line = (char *) line + 1;

    if (nesting_level == MAXDEPTH) {
        raise_error(TOO_DEEP, FATAL, NULL);
        wrapup();
    }

    subroutine_stack[nesting_level ++] = current_line;

    get_to_label(find_label(line), line);
}

void compute(char *line)
  /* Compute the indicated expression based on whether it's a
   * numerical or string expression.
   *   Possible errors: BAD_LHS, MISSING_EQ
   */
{
    struct symbol_entry *node;
    char *ident, buffer[SLEN];
    int value = 0, i = 0, j = 0;

    if (!(boolean_cont = check_condition(line)))
        return;

    init_get_token();

    if ((ident = get_token(line)) == NULL) {
        raise_error(BAD_LHS, NONFATAL, NULL);
        return;
    }

    if (!stringvar(ident) && !numvar(ident)) {
        raise_error(BAD_LHS, NONFATAL, NULL);
        return;
    }

    symbol_table = add_symbol(symbol_table, ident);
      /* Keep structure to save to */
    node = symbol_node;

    if ((ident = get_token(line)) == NULL) {
        raise_error(MISSING_EQ, NONFATAL, NULL);
        return;
    }

    if (ident[0] != '=') {
        raise_error(MISSING_EQ, NONFATAL, NULL);
        return;
    }

      /* String expression */
    if (stringvar(node->name)) {
          /* Get 'rest' of line for string substitution      */
        for (i = line_loc; !end_of_line(line[i]); i ++)
            buffer[j ++] = line[i] ++;
        buffer[j] = 0;
        if (substitute_vars(buffer) == ERROR)
            return;
        strcpy(node->value, buffer);
    }
      /* Numerical expression */
    else {
        value = evaluate(line);
        if (!error) {
            node->numvalue = value;
            sprintf(node->value, "%d", value);
        }
    }
}

int label(char *line, int loc)
  /* Add label to label table at line number loc, but only if
   * we haven't read this part of program before!
   */
{
    if (loc > furthest_into_file) {
        add_label(line);
        furthest_into_file = loc;
    }
}

int raise_error(int errno, int errtype, char *arg)
  /* Display error 'errno', type FATAL or NONFATAL, arg, if present,
   * is output too.
   */
{
    char buffer[SLEN];

    if (errtype == FATAL)
        sprintf(buffer, "FATAL Error: %s\n", errmsg[errno]);
    else
        sprintf(buffer, "Error: %s on line %d\n", errmsg[errno],
          current_line);
    printf(buffer, arg);

    return(ERROR);
}

int in_string(char *buffer, char *pattern)
  /* Returns TRUE iff pattern occurs IN IT'S ENTIRETY in buffer. */
{
    register int i = 0, j = 0;

    while (buffer[i] != '\0') {
        while (buffer[i ++] == pattern[j ++])
            if (pattern[j] == '\0')
                return(TRUE);
        i = i - j + 1;
        j = 0;
    }

    return(FALSE);
}

int check_condition(char *line)
  /* Returns non-zero iff the indicated condition (if any) is TRUE.
   * This routine will also remove the part of the  line that contains
   * the actual conditional and the colon.
   */
{
    char buffer[SLEN];

       /* Save the line */
    strcpy(buffer, line);

    if (!remove_past_colon(line))
        return(ERROR);

      /* Relational expression */
    if (buffer[0] == '(')
        return(relation(buffer));
      /* If boolean... */
    else if (buffer[0] == 'Y')
        return(boolean);
      /* If not boolean */
    else if (buffer[0] == 'N')
        return(! boolean);
      /* Always */
    else
        return(TRUE);
}

int remove_past_colon(char *line)
  /* Remove up to and including the 'colon' from input string
   * Returns zero iff no colon.
   *   Possible error: BAD_INSTRUCT
   */
{
    register int i = 0, index;

    while (line[i] != COLON && line[i] != '\0')
        i++;

    if (line[i] == COLON)
        i ++;  /* Get past colon */
      /* No colon in line!  Bad construct! */
    else
        return(raise_error(BAD_INSTRUCT, NONFATAL, line));

    for (index = i; line[index] != '\0'; index ++)
        line[index - i] = line[index];

    line[index - i] = '\0';

    return(TRUE);
}

int break_string(char *line, char *lhs, char *op, char *rhs)
  /* Breaks down line into left-hand-side, relational operator and
   * right-hand-side.  We need to strip out parens surrounding the
   * expression too without saving them.
   *   Possible errors: BAD_REL_OP, BAD_REL_EXP
   */
{
    char *word;

      /* Initialize them all */
    lhs[0] = rhs[0] = op[0] = '\0';

    init_get_token();

    if (get_token(line) == NULL)  /* No open parenthesis! */
        return(raise_error(BAD_REL_EXP, NONFATAL, NULL));

      /* Get lhs ... */
    while ((word = get_token(line)) != NULL && ! relop(word[0]) &&
      word[0] != COLON && ! paren(word[0]))
        sprintf(lhs, "%s%s", lhs, word);

    if (word == NULL)
        return(raise_error(BAD_REL_EXP, NONFATAL, NULL));

    if (paren(word[0]))  /* Nonexpression relational */
        return(NOERROR);

      /* Get op ... */
    strcpy(op, word);

      /* Get rhs ... */
    while ((word = get_token(line)) != NULL && word[0] != COLON)
        sprintf(rhs, "%s%s", rhs, word);

      /* Remove last closing paren */
    lastch(rhs) = '\0';

    if (word == NULL)
        return(raise_error(BAD_REL_EXP, NONFATAL, NULL));
    else
        return(NOERROR);
}

void add_label(char *name)
  /* Add given label to label list at end, if not found first.
   *   Possible errors: DUP_LABEL, OUT_OF_MEM
   */
{
    struct a_label *previous;

      /* Both previous and last are set to head */
    previous = last = label_list;

    while (last != NULL) {
        if (strcmp(last->name, name) == 0) {
            raise_error(DUP_LABEL, NONFATAL, name);
            return;
        }
        previous = last;
        last = last->next;
    }

      /* At this point entry == NULL and previous == last valid entry */

    if ((last = (struct a_label *) malloc(sizeof *last)) == NULL) {
          /* No memory! */
        raise_error(OUT_OF_MEM, FATAL, NULL);
        wrapup();
    }

    strncpy(last->name, name, NLEN);
    last->loc = current_line;
    last->next= NULL;

    if (previous == NULL)
        label_list = last;  /* First element in list */
    else
        previous->next = last;
}

int find_label(char *name)
  /* Returns line location of specified label or 0 if that label
   * isn't currently in the label table.
   */
{
    struct a_label *entry;

    entry = label_list; /* set entry to label list head */

    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0)
            return(entry->loc);
        entry = entry->next;
    }

    return(0);
}

int get_to_label(int loc, char *labelname)
  /* Move file pointer to indicated line number.  If loc is zero this
   * indicates that we need to scan FORWARD in the file, so reads
   * quickly forward, adding newly encountered labels to the label
   * list as encounted.  If loc is non-zero, get to specified line
   * from current line in minimal movement possible.
   *   Possible errors: NO_LABEL, UNEXPECT_EOF
   */
{
    char buffer[SLEN];

      /* Forward scan */
    if (loc == 0) {
        if (debug)
            printf("\tget_to_label(%s)\n", labelname);
        while (fgets(buffer, SLEN, fileid) != NULL) {
            remove_return(buffer);
            current_line ++;
            if (debug)
                printf("%d >> %s\n", current_line, buffer);
            if (buffer[0] == '*') {
                if (label(buffer, current_line) == ERROR) {
                    raise_error(UNEXPECT_EOF, FATAL, labelname);
                    wrapup();
                }
                if (strcmp(labelname, last_label()) == 0)
                    return;  /* Label found! */
            }
        }
        raise_error(NO_LABEL, FATAL, labelname);
        wrapup();
    }
      /* Get to specified line */
    else {
        if (loc < current_line) {  /* If before, rewind file */
            rewind(fileid);
            current_line = 0;
        }

        while (fgets(buffer, SLEN, fileid) != NULL) {
            current_line ++;
            if (current_line == loc)
                return;
        }
        raise_error(UNEXPECT_EOF, FATAL, NULL);
        wrapup();
    }
}

struct symbol_entry *add_symbol(struct symbol_entry *node, char *symbol)
  /* This routine adds the specified symbol to the symbol table.
   * The first character determines the type of the variable: '$' for
   * strings and '#' for integers.
   *   Possible errors: OUT_OF_MEM
   */
{
    int cond;

    if (node == NULL) {
        if ((node = (struct symbol_entry *) malloc(sizeof *node)) == NULL) {
            raise_error(OUT_OF_MEM, FATAL, NULL);
            wrapup();
        }
        strcpy(node->name, symbol);
        node->value[0] = '\0';
        node->numvalue = 0;
        node->left  = NULL;
        node->right = NULL;
          /* Store address globally too, if needed! */
        symbol_node = node;
    }
    else if ((cond = strcmp(symbol, node->name)) == 0)
          /* Store address globally too, if needed! */
        symbol_node = node;
    else if (cond < 0)
        node->left = add_symbol(node->left, symbol);
    else
        node->right= add_symbol(node->right, symbol);

    return(node);
}

void print_symbol_table(struct symbol_entry *node)
  /* Recursively lists all entries in the symbol table. Debug only */
{
    if (node != NULL) {
        print_symbol_table(node->left);
        printf("\t%-20.20s '%s'\n", node->name, node->value);
        print_symbol_table(node->right);
    }
}

struct symbol_entry *find_symbol(struct symbol_entry *node, char *symbol)
  /* Returns either NULL if the symbol is not found or the address of
   * the structure containing the specified symbol.  This is a stan-
   * dard recursive binary tree search...
   */
{
    int cond;

    if (node == NULL)
        return(NULL);
    else if ((cond = strcmp(symbol, node->name)) == 0)
        return(node); /* store if needed */
    else if (cond < 0)
        return(find_symbol(node->left, symbol));
    else
        return(find_symbol(node->right, symbol));
}

int substitute_vars(char *line)
   /* This routine substitutes the value for each variable it finds in
    * the given line.
    *   Possible error: UNDEF_VAR
    */
{
    struct symbol_entry *entry, *find_symbol();
    register int i = 0, j = 0, word_index;
    char word[NLEN], buffer[SLEN];

    do {
          /* While not in variable copy to buffer... */
        while (line[i] != STRINGDELIM && line[i] != NUMDELIM &&
          !end_of_line(line[i]))
            buffer[j ++] = line[i ++];

          /* Get variable if it exists... */
        word_index = 0;

        if (!end_of_line(line[i]))
          /* Copy in the delimiter */
        word[word_index ++] = line[i ++];

        while (!end_of_line(line[i]) && valid_char(line[i]))
            word[word_index ++] = line[i ++];

          /* Have a variable? If so try to find and substitute */
        if (word_index > 0) {
            word[word_index] = '\0';
            if ((entry = find_symbol(symbol_table, word)) == NULL)
                return(raise_error(UNDEF_VAR, NONFATAL, word));

            for (word_index = 0; word_index < strlen(entry->value);
              word_index ++)
                buffer[j++] = (entry->value)[word_index];
        }
    } while (!end_of_line(line[i]));

    buffer[j] = '\0';
      /* Copy it back in */
    strcpy(line, buffer);

    return(NOERROR);
}

int relation(char *exp)
  /* Evaluate relational expression between a set of parenthesis.
   * Returns TRUE or FALSE according to the results of the evaluation.
   * If an error occurs this routine will always return FALSE.
   *   Possible errors: BAD_REL_OP
   */
{
    char word[NLEN], lhs_string[SLEN], rhs_string[SLEN];
    int retval,
        lhs,      /* Left hand side */
        rhs;      /* Right hand side */

    if (break_string(exp, lhs_string, word, rhs_string) == ERROR)
        return(FALSE);    /* Default for error */

    init_get_token();             /* New string:    */
    lhs = evaluate(lhs_string);   /* left hand side */

    if (error)
        return(FALSE);  /* Erroneous always fail */

    if (word[0] == '\0' && rhs_string[0] == '\0')
        return(lhs != 0);  /* Accept no relation exp. */

    init_get_token();             /* New string:     */
    rhs = evaluate(rhs_string);   /* right hand side */

    if (error)
        return(FALSE);  /* Erroneous always fail */

      /* Compute return value   */
    switch (word[0]) {
        case '=' :  retval = (lhs == rhs);
                    break;
        case '<' :  switch (word[1]) {
                        case '\0' : retval = (lhs < rhs);
                                    break;
                        case '>'  : retval = (lhs != rhs);
                                    break;
                        case '='  : retval = (lhs <= rhs);
                                    break;
                        default   : return(raise_error(BAD_REL_OP, NONFATAL,
                                      word));
                    }
                    break;
        case '>' :  switch (word[1]) {
                        case '\0' : retval = (lhs > rhs);
                                    break;
                        case '='  : retval = (lhs >= rhs);
                                    break;
                        default   : return(raise_error(BAD_REL_OP, NONFATAL,
                                      word));
                    }
                    break;
        default  :  return(raise_error(BAD_REL_OP, NONFATAL, word));
    }

    return(retval);
}

int evaluate(char *exp)
  /* Evaluate expression and check that we've read all the tokens
   * Returns value or ERROR.
   *   Possible errors: BAD_PARENS
   */
{
    int value;

    value = expression(exp);

    if (get_token(exp) != NULL)
        return((!error ++) ? raise_error(BAD_PARENS, NONFATAL, NULL) : 0);
    else
        return(value);
}

int expression(char *exp)
  /* Recursively evaluate the expression given. */
{
    char *word;
    int val = 0;

    val = term(exp);

    if ((word = get_token(exp)) == NULL)
        return(val);

    if (!addops(word[0])) {
        unget_token();
        return(val);
    }

    while (addops(word[0])) {
        if (word[0] == PLUS)
            val += term(exp);
        else  /* Must be MINUS */
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

int term(char *exp)
  /* Get a term (ie a multiply or divide) and return the results
   * of computing it.
   *   Possible errors: DIVIDE_BY_ZERO
   */
{
    register int val = 0, value;
    char *word;

    val = factor(exp);

    /* if ((word = get_token(exp,3)) == NULL) */
    if ((word = get_token(exp)) == NULL)
        return(val);

    if (!mulops(word[0])) {
        unget_token();
        return(val);
    }

    while (mulops(word[0])) {
        if (word[0] == TIMES)
            val *= factor(exp);
        else if ((value = factor(exp)) == 0) {
            if (!error ++) {
                raise_error(DIVIDE_BY_ZERO, FATAL, NULL);
                wrapup();
            }
            else
                return(0);
        }
        else
            val /= value;

        if ((word = get_token(exp)) == NULL)
            return(val);

        if (!mulops(word[0])) {
            unget_token();
            return(val);
        }
    }

    return(val);
}

int factor(char *string)
  /* Break down a string - either another expression in parentheses,
   * a specific numerical value or a variable name
   *   Possible errors: BAD_EXP, BAD_PARENS, STRING_IN_EXP, UNDEF_VAR
   */
{
    struct symbol_entry *entry, *find_symbol();
    int val = 0;
    char *word;

    if ((word = get_token(string)) == NULL)
      return((!error ++) ? raise_error(BAD_EXP, NONFATAL, NULL) : 0);

    if (word[0] == LEFT_PAREN) {
        val = expression(string);
        if ((word = get_token(string)) == NULL)
            return((!error ++) ? raise_error(BAD_PARENS, NONFATAL, NULL) : 0);
        else if (word[0] != RIGHT_PAREN)
            return((!error ++) ? raise_error(BAD_EXP, NONFATAL, NULL) : 0);
    }
    else if (stringvar(word))  /* What's THIS doing here?? */
        return((!error ++) ? raise_error(STRING_IN_EXP, NONFATAL, NULL) : 0);
    else if (numvar(word)) {
        if ((entry = find_symbol(symbol_table, word)) == NULL)
            return((!error ++) ? raise_error(UNDEF_VAR, NONFATAL, word) : 0);
        val = entry->numvalue;
    }
    else if (word[0] == '-') {  /* Minus number */
        if ((word = get_token(string)) == NULL)
            return((!error ++) ? raise_error(BAD_EXP, NONFATAL, NULL) : 0);
        val = -atoi(word);
    }
    else
        val = atoi(word);

    return(val);
}


char *get_token(char *line)
  /* Return the next token in the line without surrounding white spaces.
   * Return zero if at end-of-line
   */
{
    static char word[SLEN];
    register int i = 0;

    while (whitespace(line[line_loc]))
        line_loc ++;

    last_line_loc = line_loc;

    if (end_of_line(line[line_loc]))
        return(NULL);

    if (mathops(line[line_loc]) || paren(line[line_loc]))
        word[i ++] = line[line_loc ++];
    else if (relop(line[line_loc])) {
        word[i ++] = line[line_loc ++];
        if (relop(line[line_loc]) && line[line_loc] != line[line_loc-1])
            word[i++] = line[line_loc++];
    }
    else {
        while (!special(line[line_loc]) && !whitespace(line[line_loc]) &&
          !end_of_line(line[line_loc]))
            if (i == NLEN-1) {
                word[i] = '\0';
                raise_error(NAME_TOO_LONG, FATAL, word);
                wrapup();
            }
            else
                word[i ++] = line[line_loc ++];
    }

    word[i] = '\0';

    return((char *) word);
}
