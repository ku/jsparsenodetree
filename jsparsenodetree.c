
//
// http://siliconforks.com/doc/parsing-javascript-with-spidermonkey/
// gcc -o jsparsenodetree -Wall -Wno-format -g -DXP_UNIX -DSVR4 -DSYSV -D_BSD_SOURCE -DPOSIX_SOURCE -DDARWIN -DX86_LINUX  -DDEBUG -DDEBUG_kuma -DEDITLINE -IDarwin_DBG.OBJ  -DHAVE_VA_COPY -DVA_COPY=va_copy t.c -L./Darwin_DBG.OBJ -ljs jsparsenodetree.c
//

#include "jsapi.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsatom.h"
#include "jsstr.h"
#include "jsregexp.h"

#include <stdio.h>
#include <stdlib.h>
const char * TOKENS[] = {
#include "tokens.def"
NULL
};


static const char *opcodenames[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
	name,
# include "jsopcode.tbl"
# undef OPDEF
};

#define NOT_HANDLED(root) \
	printf("%s/%s", arityname(root->pn_arity), TOKENS[root->pn_type] );\
	exit(-1);

void printStringCore(JSString *str, int escape) {
	size_t i, n;
	jschar *s;
	s = JSSTRING_CHARS(str);
	for (i=0, n=JSSTRING_LENGTH(str); i < n; i++) {
		if ( escape ) {
			if ( s[i] == '\\' || s[i] == '"' )
				fputc('\\', stdout);
			else if ( s[i] == '\r') {
				printf("\\\\r");
				continue;
			} else if ( s[i] == '\n') {
				printf("\\\\n");
				continue;
			}
				
		}
		fputc(s[i], stdout);
	}
}
void printStringEscape(JSString *str) {
	printStringCore(str, 1);
}
void printString(JSString *str) {
	printStringCore(str, 0);
}
const char* arityname(int arity)  {
	const char* tbl[] = {
		"FUNC"     ,
		"LIST"     ,
		"NAME"     ,
		"NULLARY"  ,
		"UNARY"    ,
		"BINARY"   ,
		"TERNARY"  
	};
	return tbl[(arity + 3)];
}

	static JSContext * context;
void print_tree(JSParseNode * root, int indent) {
	if ( root == NULL ) {
		printf("null");
		return;
	}

  switch (root->pn_arity) {
  case PN_UNARY:
		printf("\
%*s{\n\
%*s\"node\":  \"%s\",\n\
%*s\"token\": \"%s\",\n\
%*s\"kid\": ",
			indent + 0, "",
			indent + 2, "", arityname(root->pn_arity),
			indent + 2, "", TOKENS[root->pn_type],
			indent + 2, "");
    print_tree(root->pn_kid, indent + 2);
	printf("\n%*s}", indent + 0, "");
    break;
  case PN_BINARY:
		printf("\
%*s{\n\
%*s\"node\":  \"%s\",\n\
%*s\"token\": \"%s\",\n\
%*s\"opcode\": \"%s\",\n\
%*s\"left\": ",
			indent + 0, "",
			indent + 2, "", arityname(root->pn_arity),
			indent + 2, "", TOKENS[root->pn_type],
			indent + 2, "", opcodenames[root->pn_op],
			indent + 2, "");
    print_tree(root->pn_left, indent + 2);
		printf(",\n\
%*s\"right\": ",
			indent + 2, "");
		print_tree(root->pn_right, indent + 2);
		printf("\n%*s}", indent + 0, "");
    break;
  case PN_TERNARY:
		printf("\
%*s{\n\
%*s\"node\":  \"%s\",\n\
%*s\"token\": \"%s\",\n\
%*s\"kid1\": ",
			indent + 0, "",
			indent + 2, "", arityname(root->pn_arity),
			indent + 2, "", TOKENS[root->pn_type],
			indent + 2, "");
//printf("type: %d\n", root->pn_type);
	if ( root->pn_kid1 ) {
		print_tree(root->pn_kid1, indent + 2);
	} else {
		printf("null");
	}

	if ( root->pn_kid2 ) {
		printf(",\n\
%*s\"kid2\": ",
			indent + 2, "");
		print_tree(root->pn_kid2, indent + 2);
	}
	if ( root->pn_kid3 ) {
		printf(",\n\
%*s\"kid3\": ",
			indent + 2, "");
		print_tree(root->pn_kid3, indent + 2);
	}
	printf("\n%*s}", indent + 0, "");
    break;
  case PN_LIST:
    {
		printf("\
%*s{\n\
%*s\"node\":  \"%s\",\n\
%*s\"token\": \"%s\",\n\
%*s\"list\": [\n",
			indent + 0, "",
			indent + 2, "", arityname(root->pn_arity),
			indent + 2, "", TOKENS[root->pn_type],
			indent + 2, "");
    	JSParseNode * p;
		for (p = root->pn_head; p != NULL; p = p->pn_next) {
			if ( p ) {
				print_tree(p, indent + 4);
			}
			if ( p->pn_next )
				printf(",");
			printf("\n");
		}
		printf("%*s]\n%*s}",
			indent + 2, "",
			indent + 0, ""
			);
    }
    break;
  case PN_NAME:
  	{
		
		if ( root->pn_type == TOK_NAME ||
			root->pn_type == TOK_DOT
		) {
			printf("%*s{ \"node\":\"NAME\", \"token\":\"%s\", \"identifier\":\"",
			indent, "",
			TOKENS[root->pn_type]
			);
			printString( ATOM_TO_STRING(root->pn_atom) ) ;
			printf("\"");

			if ( root->pn_expr ) {
				if ( root->pn_type == TOK_NAME ) {
					printf(", \"assign\":");
					print_tree(root->pn_expr, indent + 2);
				} else {
					printf(", \"left\":");
					print_tree(root->pn_expr, indent + 2);
				}
			}
			printf("}");
		} else if ( root->pn_type == TOK_LEXICALSCOPE ) {
		// pn_op: JSOP_LEAVEBLOCK or JSOP_LEAVEBLOCKEXPR
		// pn_pob: block object
		// pn_expr: block body
			printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\", \"body\": ",
				indent, "", arityname(root->pn_arity),
				TOKENS[root->pn_type],
				opcodenames[root->pn_op]
			);
			print_tree(root->pn_expr, indent + 2);
			//printf("\"");
			printf("}");
			
			
		} else {
			NOT_HANDLED(root);
		}
	}
	break;
  case PN_NULLARY:
	if ( root->pn_type  == TOK_NUMBER ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"NUMBER\", \"value\":%f}",
		indent, "", arityname(root->pn_arity), (double)root->pn_dval);
	} else if ( root->pn_type == TOK_NAME ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"identifier\":\"",
			indent, "",  arityname(root->pn_arity),
			TOKENS[root->pn_type]
		);
		printString( ATOM_TO_STRING(root->pn_atom) ) ;
		printf("\"");
		printf("}");
	} else if ( root->pn_type == TOK_STRING ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\", \"text\": \"",
			indent, "", arityname(root->pn_arity),
			TOKENS[root->pn_type],
			opcodenames[root->pn_op]
		);
		//print_tree(root->pn_expr, indent + 2);
		printStringEscape( ATOM_TO_STRING(root->pn_atom) ) ;
	//	printString( ATOM_TO_STRING(root->pn_atom2) ) ;
		printf("\"");
		printf("}");
	} else if ( root->pn_type == TOK_PRIMARY ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\"",
			indent, "", arityname(root->pn_arity),
			TOKENS[root->pn_type],
			opcodenames[root->pn_op]
		);
		printf("}");
	} else if ( root->pn_type == TOK_XMLTEXT ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\", \"text\": \"",
			indent, "", arityname(root->pn_arity),
			TOKENS[root->pn_type],
			opcodenames[root->pn_op]
		);
		printStringEscape( ATOM_TO_STRING(root->pn_atom) ) ;
		printf("\"}");
	} else if ( root->pn_type == TOK_XMLATTR ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\", \"text\": \"",
			indent, "", arityname(root->pn_arity),
			TOKENS[root->pn_type],
			opcodenames[root->pn_op]
		);
		//print_tree(root->pn_expr, indent + 2);
		printString( ATOM_TO_STRING(root->pn_atom) ) ;
	//	printString( ATOM_TO_STRING(root->pn_atom2) ) ;
		printf("\"");
		printf("}");
		
	} else if ( root->pn_type == TOK_XMLNAME ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\", \"text\": \"",
			indent, "", arityname(root->pn_arity),
			TOKENS[root->pn_type],
			opcodenames[root->pn_op]
		);
		printString( ATOM_TO_STRING(root->pn_atom) ) ;
		printf("\"");
		printf("}");
		
	} else if ( root->pn_type == TOK_BREAK || 
				 root->pn_type == TOK_CONTINUE ) {
		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\", \"text\": \"",
			indent, "", arityname(root->pn_arity),
			TOKENS[root->pn_type],
			opcodenames[root->pn_op]
		);
		//print_tree(root->pn_expr, indent + 2);
		if ( root->pn_atom ) 
			printString( ATOM_TO_STRING(root->pn_atom) ) ;
		else
			printf("null");
		printf("\"");
		printf("}");
		
	} else if ( root->pn_type == TOK_REGEXP ) {
		JSRegExp* re = (JSRegExp *) JS_GetPrivate(context, root->pn_pob->object);

		printf("%*s{ \"node\":\"%s\", \"token\":\"%s\", \"opcode\":\"%s\", \"text\": \"",
			indent, "", arityname(root->pn_arity),
			TOKENS[root->pn_type],
			opcodenames[root->pn_op]
		);
		printStringEscape(re->source);
		printf("\"");
		printf("}");
		
	} else {
		NOT_HANDLED(root);
	}
    break;
  case PN_FUNC:
	printf("\
%*s{\n\
%*s\"node\":  \"%s\",\n\
%*s\"token\": \"%s\",\n",
			indent + 0, "",
			indent + 2, "", arityname(root->pn_arity),
			indent + 2, "", TOKENS[root->pn_type]
	);
	printf("%*s\"name\": ",
			indent + 2, ""
	);

	if ( root->pn_funpob && root->pn_funpob->object ) {
		JSFunction* fun = (JSFunction *) JS_GetPrivate(context, root->pn_funpob->object);
		const char* name = JS_GetFunctionName(fun);
		printf("\"%s\",\n", name);
	} else {
		
		printf("null,\n");
	}
/*
	if ( root->pn_funAtom ) {
		JSFunction* fun = (JSFunction *) JS_GetPrivate(context, ATOM_TO_OBJECT(root->pn_funAtom));
	} else {
	}
*/
	printf("\
%*s\"body\": [\n",
			indent + 2, "");
    print_tree(root->pn_body, indent + 2);

		printf("%*s]\n%*s}",
			indent + 2, "",
			indent + 0, ""
		);
	break;
  default:
    fprintf(stderr, "Unknown node type %d\n", root->pn_arity);
    exit(EXIT_FAILURE);
    break;
  }
	
}

#define SPIDERMONKEY180

int main (int ac, char* av[]) {
	JSRuntime * runtime;
	JSObject * global;

	runtime = JS_NewRuntime(8L * 1024L * 1024L);
	if (runtime == NULL) {
		fprintf(stderr, "cannot create runtime");
		exit(EXIT_FAILURE);
	}

	context = JS_NewContext(runtime, 8192);
	if (context == NULL) {
		fprintf(stderr, "cannot create context");
		exit(EXIT_FAILURE);
	}
	//JS_SetVersion(context, JSVERSION_DEFAULT );

	global = JS_NewObject(context, NULL, NULL, NULL);
	if (global == NULL) {
		fprintf(stderr, "cannot create global object");
		exit(EXIT_FAILURE);
	}

	if (! JS_InitStandardClasses(context, global)) {
		fprintf(stderr, "cannot initialize standard classes");
		exit(EXIT_FAILURE);
	}

	JSParseNode * node;

	if ( ac < 2 )
		exit(1);

	FILE* fp;
	if ( (fp= fopen(av[1], "rt")) == NULL )
		exit(2);

#ifdef SPIDERMONKEY170
	JSTokenStream * token_stream;
	token_stream = js_NewFileTokenStream(context, NULL, fp);
	if (token_stream == NULL) {
		fprintf(stderr, "cannot create token stream from file\n");
	}

	node = js_ParseTokenStream(context, global, token_stream);
	if (node == NULL) {
		fprintf(stderr, "parse error in file\n");
		exit(EXIT_FAILURE);
	}
#endif
#ifdef SPIDERMONKEY180
	JSParseContext pc;

	fseek(fp, 0L, SEEK_END);
	long n = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	char* chars = (char*)malloc(n + 1);
	fread(chars, 1, n, fp);
	chars[n] = 0;

	size_t length = n;
	jschar* jschars = js_InflateString(context, chars, &length);
	
	if (js_InitParseContext(context, &pc, NULL, jschars, length, NULL, NULL, 1)) {
		node = js_ParseScript(context, global, &pc);
		JS_free(context, jschars);
	} else {
		exit(EXIT_FAILURE);
	}
#endif
	print_tree(node, 0);

	return 0;
}

