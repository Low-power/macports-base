/*
 * readline.c
 *
 * Some basic readline support callable from Tcl
 * By James D. Berry <jberry@opendarwin.org> 10/27/05
 *
 * $Id: readline.c,v 1.1.2.1 2005/11/08 05:42:58 jberry Exp $
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>

#if HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#endif

#if HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif

#include <tcl.h>
#include "readline.h"


/* Globals */
#if HAVE_READLINE_READLINE_H
Tcl_Interp* completion_interp = NULL;
Tcl_Obj* attempted_completion_word = NULL;
Tcl_Obj* generator_word = NULL;
#endif


char*
completion_generator(const char* text, int state)
{
	const char* match = NULL;
	if (completion_interp && generator_word) {
		Tcl_Obj* objv[4];
		objv[0] = generator_word;
		objv[1] = Tcl_NewStringObj(text, -1);
		objv[2] = Tcl_NewIntObj(state);
		objv[3] = NULL;
		
		if (TCL_OK == Tcl_EvalObjv(completion_interp, 3, objv, TCL_EVAL_DIRECT)) {
			match = Tcl_GetStringResult(completion_interp);
		}
	}
	
	return (match && *match) ? strdup(match) : NULL;
}

	
char**
attempted_completion_function(const char* word, int start, int end)
{
	/*
		If we can complete the text at start/end, then
		call rl_completion_matches with a generator function,
		else return NULL.
		
		We call:
			attempted_completion_word line_buffer word start end
			
		If it returns a null string, then we return NULL,
		otherwise, we use the string returned as a proc name
		to call into to generate matches
	*/
	
	char** matches = NULL;
	
	if (completion_interp && attempted_completion_word) {
	
		Tcl_Obj* objv[6];
		objv[0] = attempted_completion_word;
		objv[1] = Tcl_NewStringObj(rl_line_buffer, -1);
		objv[2] = Tcl_NewStringObj(word, -1);
		objv[3] = Tcl_NewIntObj(start);
		objv[4] = Tcl_NewIntObj(end);
		objv[5] = NULL;
		
		if (TCL_OK == Tcl_EvalObjv(completion_interp, 5, objv, TCL_EVAL_DIRECT)) {
			/* If the attempt proc returns a word result, it's the
			   word to call as a generator function
			 */
			generator_word = Tcl_GetObjResult(completion_interp);
			if (generator_word && Tcl_GetCharLength(generator_word)) {
				Tcl_IncrRefCount(generator_word);
#if HAVE_LIBREADLINE
				matches = completion_matches(word, completion_generator);
#endif
				Tcl_DecrRefCount(generator_word);
			}
		}
	}
	
	return matches;
}


/*
	readline action
	
	actions:
		init ?name?
		read line ?prompt?
		read -attempted_completion proc line ?prompt?
		completion_matches text function
*/
int ReadlineCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	char* action;
	Tcl_Obj *tcl_result;
	int argbase;
	int argcnt;
	
	/* Get the action */
	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "action");
		return TCL_ERROR;
	}
	action = Tcl_GetString(objv[1]);

	/* Case out on action */
	if        (0 == strcmp("init", action)) {
	
		int initOk = 0;
		
#if HAVE_LIBREADLINE
		/* Set the name of our program, so .inputrc can be conditionalized */
		if (objc == 3) {
			rl_readline_name = strdup(Tcl_GetString(objv[2]));
		} else if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, "init");
			return TCL_ERROR;
		}

		/* Initialize history */
		using_history();

		/* Setup for completion */
		rl_attempted_completion_function = attempted_completion_function;

		initOk = 1;		
#endif
	
		tcl_result = Tcl_NewIntObj(initOk);	
		Tcl_SetObjResult(interp, tcl_result);

#if HAVE_LIBREADLINE
	} else if (0 == strcmp("read", action)) {
	
		char* s;
		char* line;
		char* line_name;
		char* prompt = "default prompt: ";
		int line_len;
		
		/* Initialize completion stuff */
		completion_interp = interp;
		attempted_completion_word = NULL;
		generator_word = NULL;
		
		/* Process optional parameters */
		for (argbase = 2; argbase < objc; ) {
			s = Tcl_GetString(objv[argbase]);
			if (!s || s[0] != '-')
				break;
			++argbase;
			
			if (0 == strcmp("-attempted_completion", s)) {
				if (argbase >= objc) {
					Tcl_WrongNumArgs(interp, 1, objv, "-attempted_completion");
					return TCL_ERROR;
				}
				attempted_completion_word = objv[argbase++];
			} else {
				Tcl_AppendResult(interp, "Unsupported argument: ", s, NULL);
				return TCL_ERROR;
			}
		}
		argcnt = objc - argbase;
		
		/* Pick a prompt */
		if (argcnt == 2) {
			prompt = Tcl_GetString(objv[argbase + 1]);
		} else if (argcnt != 1) {
			Tcl_WrongNumArgs(interp, 1, objv, "read -arg... line ?prompt?");
			return TCL_ERROR;
		}
	
		/* Read the line */
		line = readline(prompt);
		line_len = (line == NULL) ? -1 : (int)strlen(line);
	
		line_name = Tcl_GetString(objv[argbase + 0]);
		Tcl_SetVar(interp, line_name, (line == NULL) ? "" : line, 0); 
		free(line);
		
		tcl_result = Tcl_NewIntObj(line_len);	
		Tcl_SetObjResult(interp, tcl_result);
	
#endif
	} else {
	
		Tcl_AppendResult(interp, "Unsupported action: ", action, NULL);
		return TCL_ERROR;
		
	}

	return TCL_OK;
}


/*
	history action
	
	action:
		add line
		read filename
		write filename
		stifle max
		unstifle
*/
int HistoryCmd(ClientData clientData UNUSED, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
	char* action = NULL;
	char* s = NULL;
	int i = 0;
	Tcl_Obj *tcl_result;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "action");
		return TCL_ERROR;
	}
	action = Tcl_GetString(objv[1]);

	/* Case out on action */
	if (0) {
#if HAVE_LIBREADLINE
	} else if (0 == strcmp("add", action)) {
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 1, objv, "add line");
			return TCL_ERROR;
		}
		s = Tcl_GetString(objv[2]);
		add_history(s);
	} else if (0 == strcmp("read", action)) {
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 1, objv, "read filename");
			return TCL_ERROR;
		}
		s = Tcl_GetString(objv[2]);
		read_history(s);
	} else if (0 == strcmp("write", action)) {
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 1, objv, "write filename");
			return TCL_ERROR;
		}
		s = Tcl_GetString(objv[2]);
		write_history(s);
	} else if (0 == strcmp("stifle", action)) {
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 1, objv, "stifle maxlines");
			return TCL_ERROR;
		}
		if (TCL_OK == Tcl_GetIntFromObj(interp, objv[2], &i))
			stifle_history(i);
	} else if (0 == strcmp("unstifle", action)) {
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, "unstifle");
			return TCL_ERROR;
		}
		i = unstifle_history();
		tcl_result = Tcl_NewIntObj(i);
		Tcl_SetObjResult(interp, tcl_result);
#endif
	} else {
		Tcl_AppendResult(interp, "Unsupported action: ", action, NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
}
