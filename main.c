#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char* regex_to_postscript(char* regex) {
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

	if(strlen(regex) >= sizeof buffer/2) {
		return NULL;
	}
	for(; *regex; regex++) {
		switch(*regex) {
			case '(':
				if(num_atomics > 1) {
				// if we have atomics pending, add a concatenation operator to the buffer
					--num_atomics;
					*buffer_ptr++ = '.';
				}
				if(b >= brackets + 100) {
					return NULL;
				}
				b->num_ors = num_ors;
				b->num_atomics = num_atomics;
				break;
			case '|':
				if(num_atomics == 0) {
					return NULL;
				}
				while(--num_atomics > 0) {
					*buffer_ptr++ = '.';
				}
				num_ors++;
				break;
			case ')':
				if(b == brackets) {
					return NULL;
				}
				if(num_atomics == 0) {
					return NULL;
				}
				while(--num_atomics > 0) {
					*buffer_ptr++ = '.';
				}
				for(; num_ors > 0; num_ors--) {
					*buffer_ptr++ = '|';
				}
				--b;
				num_ors = b->num_ors;
				num_atomics = b->num_atomics;
				num_atomics++;
				break;
			case '*':
			case '+':
				if(num_atomics > 1) {
					--num_atomics;
					*buffer_ptr++ = '.';
				}
				*buffer_ptr++ = *regex;
				break;
			default:
				if(num_atomics > 1) {
					--num_atomics;
					*buffer_ptr++ = '.';
				}
				*buffer_ptr++ = *regex;
				num_atomics++;
				break;
		}
			
	}
	if(b != brackets) {
		return NULL;
	}
	while(--num_atomics > 0) {
		*buffer_ptr++ = '|';
	}
	*buffer_ptr = 0;
	return buffer;
}

int main(int argc, char *argv[]) {
	char *result = regex_to_postscript("a|b");
	printf("%s\n", result);
}
