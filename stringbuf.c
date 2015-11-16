#include "stringbuf.h"

//Function definitions for the prototypes in the header file
stringbuf * stringbuf_create()
{
	stringbuf * buf = malloc(sizeof(stringbuf));
	if (buf) {
		buf->size = 16;
		buf->len = 0;
		buf->buf = malloc(buf->size);
		if (buf->buf) 
			buf->buf[0] = '\0';
	}
	return buf;
}

void stringbuf_delete(stringbuf * buf)
{
	if (buf) {
		free(buf->buf);
		free(buf);
	}
}

void stringbuf_resize(stringbuf * buf, size_t newsize)
{
	void *temp = realloc(buf->buf, newsize);
	if (temp) 
		buf->size = newsize;
	else 
		free(buf->buf);
	buf->buf = temp;
}

void stringbuf_addstring(stringbuf * buf, const char * s)
{
	const size_t len = strlen(s);
	if (buf->len + len + 1 > buf->size) 
		stringbuf_resize(buf, buf->size + len);
	if (buf->buf) {
		strncat(buf->buf, s, len);
		buf->len += len;
	}
}

void stringbuf_addchar(stringbuf * buf, char c)
{
	if (buf->len == buf->size - 1) 
		stringbuf_resize(buf, buf->size * 2);
	if (buf->buf) {
		buf->buf[buf->len] = c;
		buf->buf[buf->len + 1] = '\0';
		buf->len++;
	}
}