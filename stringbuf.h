//Include Guards
#ifndef STRINGBUF_H_   
#define STRINGBUF_H_
#endif

#include<stdlib.h>

//A growable string - Basically a user-defined implementation for String Buffer
typedef struct
{
	char *buf;
	size_t size;
	size_t len;
} stringbuf;

stringbuf * stringbuf_create();
void stringbuf_delete(stringbuf *);
void stringbuf_resize(stringbuf *, size_t);
void stringbuf_addstring(stringbuf *, const char *);
void stringbuf_addchar(stringbuf *, char);
