#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
char* regex_to_postfix(char* regex) {
        int num_ors, num_atomics;
        static char buffer[8000];
        char* buffer_ptr;

        struct {
                int num_ors;
                int num_atomics;
        } brackets[100], *b;

        b = brackets;
        buffer_ptr = buffer;
        num_ors = 0;
        num_atomics = 0;

        if (strlen(regex) >= sizeof buffer / 2) {
                return NULL;
        }
        for (; *regex; regex++) {
                switch (*regex) {
                        case '(':
                                if (num_atomics > 1) {
                                        --num_atomics;
                                        *buffer_ptr++ = '.';
                                }
                                if (b >= brackets + 100) {
                                        return NULL;
                                }
                                b->num_ors = num_ors;
                                b->num_atomics = num_atomics;
                                b++;
                                num_ors = 0;
                                num_atomics = 0;
                                break;
                        case '|':
                                if (num_atomics == 0) {
                                        return NULL;
                                }
                                while (--num_atomics > 0) {
                                        *buffer_ptr++ = '.';
                                }
                                num_ors++;
                                break;
                        case ')':
                                if (b == brackets) {
                                        return NULL;
                                }
                                if (num_atomics == 0) {
                                        return NULL;
                                }
                                while (--num_atomics > 0) {
                                        *buffer_ptr++ = '.';
                                }
                                for (; num_ors > 0; num_ors--) {
                                        *buffer_ptr++ = '|';
                                }
                                --b;
                                num_ors = b->num_ors;
                                num_atomics = b->num_atomics;
                                num_atomics++;
                                break;
                        case '*':
                        case '+':
                        case '?':
                                if (num_atomics == 0) {
                                        return NULL;
                                }
                                *buffer_ptr++ = *regex;
                                break;
                        default:
                                if (num_atomics > 1) {
                                        --num_atomics;
                                        *buffer_ptr++ = '.';
                                }
                                *buffer_ptr++ = *regex;
                                num_atomics++;
                                break;
                }
        }
        if (b != brackets) {
                return NULL;
        }
        while (--num_atomics > 0) {
                *buffer_ptr++ = '.';
        }
        for (; num_ors > 0; num_ors--) {
                *buffer_ptr++ = '|';
        }
        *buffer_ptr = 0;
        return buffer;
}

enum {
	Match=256,
	Split = 257
};

typedef struct State State;
struct State {
	int c;
	State *out;
	State *out1;
	int lastlist;
};

State matchstate = { Match };
int nstate;

State* state(int c, State *out, State *out1) {
	State *s;

	nstate++;
	s = malloc(sizeof *s);
	s->lastlist = 0;
	s->c = c;
	s->out = out;
	s->out1 = out1;
	return s;
}

typedef struct Frag Frag;
typedef union Ptrlist Ptrlist;

struct Frag {
	State *start;
	Ptrlist *out;
};

Frag frag(State *start, Ptrlist *out) {
	Frag n = { start, out };
	return n;
}

union Ptrlist {
	Ptrlist *next;
	State *s;
};

Ptrlist* list1(State **outp) {
	Ptrlist *l;
	
	l = (Ptrlist*)outp;
	l->next = NULL;
	return l;
}

void patch(Ptrlist *l, State *s) {
	Ptrlist *next;

	for(; l; l=next) {
		next = l->next;
		l->s = s;
	}
}

Ptrlist* append(Ptrlist *l1, Ptrlist *l2) {
	Ptrlist *oldl1;
	oldl1 = l1;

	oldl1 = l1;

	while(l1->next) {
		l1 = l1->next;
	}
	l1->next = l2;
	return oldl1;
}

State* postfix_to_nfa(char* postfix) {
	char *p;
	Frag stack[1000], *stackp, e1, e2, e;
	State *s;

	// fprintf(stderr, "postfix: %s\n", postfix);

	if(postfix == NULL)
		return NULL;

	#define push(s) *stackp++ = s
	#define pop() *--stackp

	stackp = stack;
	for(p=postfix; *p; p++){
		switch(*p){
		default:
			s = state(*p, NULL, NULL);
			push(frag(s, list1(&s->out)));
			break;
		case '.':	/* catenate */
			e2 = pop();
			e1 = pop();
			patch(e1.out, e2.start);
			push(frag(e1.start, e2.out));
			break;
		case '|':	/* alternate */
			e2 = pop();
			e1 = pop();
			s = state(Split, e1.start, e2.start);
			push(frag(s, append(e1.out, e2.out)));
			break;
		case '?':	/* zero or one */
			e = pop();
			s = state(Split, e.start, NULL);
			push(frag(s, append(e.out, list1(&s->out1))));
			break;
		case '*':	/* zero or more */
			e = pop();
			s = state(Split, e.start, NULL);
			patch(e.out, s);
			push(frag(s, list1(&s->out1)));
			break;
		case '+':	/* one or more */
			e = pop();
			s = state(Split, e.start, NULL);
			patch(e.out, s);
			push(frag(e.start, list1(&s->out1)));
			break;
		}
	}
	e = pop();
	if(stackp != stack) {
		return NULL;
	}
	patch(e.out, &matchstate);
	return e.start;

#undef pop
#undef push
}

typedef struct List List;
struct List {
	State **s;
	int n;
};
List l1, l2;
static int listid;

void addstate(List*, State*);
void step(List*, int, List*);

List* startlist(State *start, List *l) {
	l->n = 0;
	listid++;
	addstate(l, start);
	return l;
}

int ismatch(List *l) {
	int i;

	for(i = 0; i < l->n; i++) {
		if(l->s[i] == &matchstate) {
			return 1;
		}
	}
	return 0;
}

void addstate(List *l, State *s) {
	if(s == NULL || s->lastlist == listid) {
		return;
	}
	s->lastlist = listid;
	if(s->c == Split) {
		addstate(l, s->out);
		addstate(l, s->out1);
	}
	l->s[l->n++] = s;
}

void step(List *clist, int c, List *nlist) {
	int i;
	State *s;

	listid++;
	nlist->n = 0;
	for(i = 0; i < clist->n; i++) {
		s = clist->s[i];
		if(s->c == c) {
			addstate(nlist, s->out);
		}
	}
}

int match(State *start, char *s) {
	int i, c;
	List *clist, *nlist, *t;

	clist = startlist(start, &l1);
	nlist = &l2;
	for(; *s; s++) {
		c = *s & 0xFF;
		step(clist, c, nlist);
		t = clist; clist = nlist; nlist = t;
	}
	return ismatch(clist);
}
	

int main(int argc, char *argv[]) {
	int i;
	char *post;
	State *start;

	if(argc < 3) {
		fprintf(stderr, "Usage: NFA REGEXP STRING...\n");
		return 1;
	}

	post = regex_to_postfix(argv[1]);
	if(post == NULL) {
		fprintf(stderr, "BAD REGEXP %s\n", argv[1]);
		return 1;
	}

	start = postfix_to_nfa(post);

	if(start == NULL) {
		fprintf(stderr, "ERROR converting postfix to NFA %s\n", post);
		return 1;
	}

	l1.s = malloc(nstate*sizeof l1.s[0]);
	l2.s = malloc(nstate*sizeof l2.s[0]);
	for(i = 2; i < argc; i++) {
		if(match(start, argv[i])) {
			printf("%s\n", argv[i]);
		}
	}
	return 0;
}
