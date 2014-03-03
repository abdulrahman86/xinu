/* shell.c  -  shell */

#include <xinu.h>
#include <stdio.h>
#include "shprototypes.h"

/************************************************************************/
/* Xinu shell commands and the function associated with each		*/
/************************************************************************/
const	struct	cmdent	cmdtab[] = {
	{"argecho",	TRUE,	xsh_argecho},
	{"clear",	TRUE,	xsh_clear},
	{"devdump",	FALSE,	xsh_devdump},
	{"echo",	FALSE,	xsh_echo},
	{"exit",	TRUE,	xsh_exit},
	{"help",	FALSE,	xsh_help},
	{"kill",	TRUE,	xsh_kill},
	{"led",		FALSE,	xsh_led},
	{"memdump",	FALSE,	xsh_memdump},
	{"memstat",	FALSE,	xsh_memstat},
	{"ps",		FALSE,	xsh_ps},
	{"sleep",	FALSE,	xsh_sleep},
	{"uptime",	FALSE,	xsh_uptime},
	{"?",		FALSE,	xsh_help},
	{"gen",		FALSE,	xsh_gen},
	{"search",	FALSE,	xsh_search}
};

uint32	ncmd = sizeof(cmdtab) / sizeof(struct cmdent);

/************************************************************************/
/* Xinu shell - provide an interactive user interface that executes	*/
/*		commands.  Each command begins with a command name, has	*/
/*		a set of optional arguments, has optional input or	*/
/*		output redirection, and an optional specification for	*/
/*		background execution (ampersand).  The syntax is:	*/
/*									*/
/*		   command_name [args*] [redirection] [&]		*/
/*									*/
/*		Redirection is either or both of:			*/
/*									*/
/*				< input_file				*/
/*			or						*/
/*				> output_file				*/
/*									*/
/************************************************************************/

process	shell (
		did32	dev		/* ID of tty device from which	*/
	)				/*  to accept commands		*/
{
	char	buf[SHELL_BUFLEN];	/* input line (large enough for	*/
					/*  one line from a tty device	*/
	int32	len;			/* length of line read		*/
	char	tokbuf[SHELL_BUFLEN +	/* buffer to hold a set of	*/
			SHELL_MAXTOK];  /* contiguous null-terminated	*/
					/* strings of tokens		*/
	int32	tlen;			/* current length of all data	*/
					/*   in array tokbuf		*/
	int32	tok[SHELL_MAXTOK];	/* index of each token in	*/
					/*   tokbuf			*/
	int32	toktyp[SHELL_MAXTOK];	/* type of each token in tokbuf	*/
	int32	ntok;			/* number of tokens on line	*/
	pid32	child;			/* process ID of spawned child	*/
	bool8	backgnd;		/* run command in background?	*/
	char	*outname, *inname;	/* ptrs to strings for file	*/
					/*   names that follow > and <	*/
	did32	stdinput, stdoutput;	/* descriptors for redirected	*/
					/*   input and output		*/
	int32	i;			/* index into array of tokens	*/
	int32	j;			/* index into array of commands	*/
	int32	k;			/* index into array of commands	*/
	int32	msg;			/* message from receive() for	*/
					/*   child termination		*/
	int32	tmparg;			/* address of this var is used	*/
					/*   when first creating child	*/
					/*   process, but is replaced	*/
	char	*src, *cmp, *cmp2, *temp;		/* ptrs using during name	*/
					/*   comparison			*/
	bool8	diff;			/* was difference found during	*/
					/*   comparison			*/
	char	*args[SHELL_MAXTOK];	/* argument vector passed to	*/
					/*   builtin commands		*/

	/* Print shell banner and startup message */

	fprintf(dev, "\n\n%s%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		SHELL_BAN0,SHELL_BAN1,SHELL_BAN2,SHELL_BAN3,SHELL_BAN4,
		SHELL_BAN5,SHELL_BAN6,SHELL_BAN7,SHELL_BAN8,SHELL_BAN9);

	fprintf(dev, "%s\n\n", SHELL_STRTMSG);

	/* Continually prompt the user, read input, and execute command	*/

	while (TRUE) {

		/* Display prompt */

		fprintf(dev, SHELL_PROMPT);

		/* Read a command (for tty, 0 means entire line) */

		len = read(dev, buf, sizeof(buf));

		/* Exit gracefully on end-of-file */

		if (len == EOF) {
			break;
		}

		/* If line contains only NEWLINE, go to next line */

		if (len <= 1) {
			fprintf(dev,"\n");
			continue;
		}

		buf[len] = SH_NEWLINE;	/* terminate line */

		/* Parse input line and divide into tokens */

		ntok = lexan(buf, len, tokbuf, &tlen, tok, toktyp);
		
		/* Handle parsing error */

		if (ntok == SYSERR) {
			fprintf(dev,"%s\n", SHELL_SYNERRMSG);
			continue;
		}

		/* If line is empty, go to next input line */

		if (ntok == 0) {
			fprintf(dev, "\n");
			continue;
		}

		/* If last token is '&', set background */

		if (toktyp[ntok-1] == SH_TOK_AMPER) {
			ntok-- ;
			tlen-= 2;
			backgnd = TRUE;
		} else {
			backgnd = FALSE;
		}


		/* Check for input/output redirection (default is none) */

		outname = inname = NULL;
		if ( (ntok >=3) && ( (toktyp[ntok-2] == SH_TOK_LESS)
				   ||(toktyp[ntok-2] == SH_TOK_GREATER))) {
			if (toktyp[ntok-1] != SH_TOK_OTHER) {
				fprintf(dev,"%s\n", SHELL_SYNERRMSG);
				continue;
			}
			if (toktyp[ntok-2] == SH_TOK_LESS) {
				inname =  &tokbuf[tok[ntok-1]];
			} else {
				outname = &tokbuf[tok[ntok-1]];
			}
			ntok -= 2;
			tlen = tok[ntok] - 1;
		}


		if ( (ntok >=3) && ( (toktyp[ntok-2] == SH_TOK_LESS)
				   ||(toktyp[ntok-2] == SH_TOK_GREATER))) {
			if (toktyp[ntok-1] != SH_TOK_OTHER) {
				fprintf(dev,"%s\n", SHELL_SYNERRMSG);
				continue;
			}
			if (toktyp[ntok-2] == SH_TOK_LESS) {
				if (inname != NULL) {
				    fprintf(dev,"%s\n", SHELL_SYNERRMSG);
				    continue;
				}
				inname = &tokbuf[tok[ntok-1]];
			} else {
				if (outname != NULL) {
				    fprintf(dev,"%s\n", SHELL_SYNERRMSG);
				    continue;
				}
				outname = &tokbuf[tok[ntok-1]];
			}
			ntok -= 2;
			tlen = tok[ntok] - 1;
		}

		cmp2 = NULL;
		if (ntok == 3 && toktyp[1] == SH_TOK_PIPE){
			cmp2 = &tokbuf[tok[2]];
			ntok -= 2;
		}

		/* Verify remaining tokens are type "other" */

		for (i=0; i<ntok; i++) {
			if (toktyp[i] != SH_TOK_OTHER) {
				break;
			}
		}
		if ((ntok == 0) || (i < ntok)) {
			fprintf(dev, SHELL_SYNERRMSG);
			continue;
		}

		stdinput = stdoutput = dev;

		/* Lookup first token in the command table */

		for (j = 0; j < ncmd; j++) {
			src = cmdtab[j].cname;
			cmp = tokbuf;
			diff = FALSE;
			while (*src != NULLCH) {
				if (*cmp != *src) {
					diff = TRUE;
					break;
				}
				src++;
				cmp++;
			}
			if (diff || (*cmp != NULLCH)) {
				continue;
			} else {
				break;
			}
		}

		/* Handle command not found */

		if (j >= ncmd) {
			fprintf(dev, "command %s not found\n", tokbuf);
			continue;
		}

		// if has a second command
		if (cmp2 != NULL){
			temp = cmp2;
			for (k = 0; k < ncmd; k++) {
				src = cmdtab[k].cname;
				cmp2 = temp;
				diff = FALSE;
				while (*src != NULLCH) {
					if (*cmp2 != *src) {
						diff = TRUE;
						break;
					}
					src++;
					cmp2++;
				}
				if (diff || (*cmp2 != NULLCH)) {
					continue;
				} else {
					break;
				}
			}

			if (k >= ncmd) {
				fprintf(dev, "command %s not found\n", temp);
				continue;
			}
		}


		/* Handle built-in command */

		if (cmdtab[j].cbuiltin) { /* no background or redirection */
			if (inname != NULL || outname != NULL || backgnd) {
				fprintf(dev, SHELL_BGERRMSG);
				continue;
			} else {
				/* Set up arg vector for call */

				for (i=0; i<ntok; i++) {
					args[i] = &tokbuf[tok[i]];
				}

				/* Call builtin shell function */

				if ((*cmdtab[j].cfunc)(ntok, args)
							== SHELL_EXIT) {
					break;
				}
			}
			continue;
		}

		// if has "|", spawn the second command
		if (cmp2 != NULL){
			pid32 child2;
			pipid32 pip;

			pip = pipcreate();
			
			if (SYSERR == pip){
				fprintf(dev, "Unable to create pipeline\n");
				continue;
			}
			
			child = create(cmdtab[j].cfunc, SHELL_CMDSTK, SHELL_CMDPRIO, cmdtab[j].cname, 2, 1, pip);
			child2 = create(cmdtab[k].cfunc, SHELL_CMDSTK, SHELL_CMDPRIO, cmdtab[k].cname, 2, 1, pip);
			
			if (SYSERR == pipconnect(pip, child, child2)) {
				fprintf(dev, "Unable to connect\n");
				continue;
			}

			fprintf(dev,"[main]: Pipe connected!\r\n");
			
			resume(child);
			resume(child2);
			continue;
		}


		/* Spawn child thread for non-built-in commands */

		child = create(cmdtab[j].cfunc,
			SHELL_CMDSTK, SHELL_CMDPRIO,
			cmdtab[j].cname, 2, ntok, &tmparg);
		

		
		/* If creation or argument copy fails, report error */

		if ((child == SYSERR) ||
		    (addargs(child, ntok, tok, tlen, tokbuf, &tmparg)
							== SYSERR) ) {
			fprintf(dev, SHELL_CREATMSG);
			continue;
		}

		/* Set stdinput and stdoutput in child to redirect I/O */

		proctab[child].prdesc[0] = stdinput;
		proctab[child].prdesc[1] = stdoutput;

		msg = recvclr();
		resume(child);
		if (! backgnd) {
			msg = receive();
			while (msg != child) {
				msg = receive();
			}
		}
    }

    /* Close shell */

    fprintf(dev,SHELL_EXITMSG);
    return OK;
}

