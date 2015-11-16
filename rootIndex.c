//Include Guards
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include<process.h>
#include<stdio.h>
#include<winsock.h>
#include<sys/stat.h>
#include<conio.h>
#include "stringbuf.h"

#define BUFSIZE 4096 //Used for recv and file transfers

//HTTP Error Messages
#define BAD_REQUEST           "400: Bad Request"
#define NOT_FOUND             "404: Not Found"
#define METHOD_NOT_ALLOWED    "405: Method Not Allowed"
#define INTERNAL_SERVER_ERROR "500: Internal Server Error"
#define FORBIDDEN			  "403: Forbidden"

//Global Declarations for server socket, directory string and directory listing permission controller
unsigned int PORT = 8080;
SOCKET sock;
char *directory = ".";
int dirPerm = 1;

//User-defined struct for mimetype descriptor
typedef struct {
	char *extension;
	char *mimetype;
} mimetype_assoc;
mimetype_assoc mimetypes[] = {
	//Some mimetypes, you can add more here
	{ "html", "text/html" },
	{ "htm", "text/html" },
	{ "xml", "application/xml" },
	{ "txt", "text/plain" },
	{ "gif", "image/gif" },
	{ "jpg", "image/jpeg" },
	{ "png", "image/png" },
	{ "bmp", "image/bmp" },
	{ "doc", "application/msword" },
	{ "xls", "application/vnd.msexcel" },
	{ "pdf", "application/pdf" },
	{ "ps", "application/postscript" },
};
const unsigned int num_mimetypes = sizeof(mimetypes) / sizeof(mimetype_assoc);

//Parse the request start line 
static unsigned int parse_start(const char *line, char **method, char **path)
{
	unsigned int result = 0; //If successful
	unsigned int i;
	unsigned int start = 0;
	for (i = start; line[i] && line[i] != ' '; i++);
	if (line[i] && i > 0) {
		*method = malloc(i + 1);
		if (*method) {
			strncpy(*method, line, i);
			(*method)[i] = '\0';
		}
		else 
			result = 2; //If Server Error
	}
	else 
		result = 1; //If Bad request 
	if (result == 0) {
		start = i + 1;
		for (i = start; line[i] && line[i] != ' '; i++);
		if (line[i] && i - start > 0) {
			*path = malloc(i - start + 1);
			if (path) {
				strncpy(*path, &line[start], i - start);
				(*path)[i - start] = '\0';
			}
			else 
				result = 2;
		}
		else 
			result = 1;
	}
	return result;
}

//Send a status message
static void send_message(const char *message, SOCKET sock)
{
	stringbuf *headers = stringbuf_create();
	stringbuf_addstring(headers, "HTTP/1.0 ");
	stringbuf_addstring(headers, message);
	stringbuf_addstring(headers, "\n\n");
	if (!headers->buf) {
		send_message(INTERNAL_SERVER_ERROR, sock);
		return;
	}
	send(sock, headers->buf, headers->len, 0);
	stringbuf_delete(headers);
}


//Send a directory listing
static void send_directory(const char *localpath, const char *uripath, SOCKET sock)
{
	stringbuf *headers = stringbuf_create();
	stringbuf *body = stringbuf_create();
	char sizebuf[128];
	char *pattern;
	//Create the pattern
	pattern = malloc(strlen(localpath) + 3);
	if (!pattern) {
		send_message(INTERNAL_SERVER_ERROR, sock);
		return;
	}
	sprintf(pattern, "%s\\*", localpath);
	printf("Pattern is %s\n", pattern);
	//Start creating the headers
	stringbuf_addstring(headers, "HTTP/1.0 200: OK\n");
	stringbuf_addstring(headers, "Content-Type: text/html\n");
	//Create the body 
	stringbuf_addstring(body, "<html>\n");
	stringbuf_addstring(body, "<h1>Directory of ");
	stringbuf_addstring(body, uripath);
	stringbuf_addstring(body, "</h1>\n\n\n\nDirectory listing will be shown here.\n");
	free(pattern);
	stringbuf_addstring(body, "</html>");
	//Complete the headers with Content-Length
	sprintf(sizebuf, "Content-Length: %d\n", body->len);
	stringbuf_addstring(headers, sizebuf);
	stringbuf_addchar(headers, '\n');
	if (!headers->buf || !body->buf) {
		send_message(INTERNAL_SERVER_ERROR, sock);
		return;
	}
	//Send
	send(sock, headers->buf, headers->len, 0);
	send(sock, body->buf, body->len, 0);
	//Cleanup
	stringbuf_delete(body);
	stringbuf_delete(headers);
}

//Add the mime-type to headers
static void add_mimetype(const char *name, stringbuf *headers)
{
	char *mimetype = NULL;
	const size_t len = strlen(name);
	const char * extension = strrchr(name, '.');
	if (extension && *(extension + 1)) {
		unsigned int e;
		extension++;
		for (e = 0; e < num_mimetypes && !mimetype; e++) {
			if (strcmp(extension, mimetypes[e].extension) == 0) {
				mimetype = mimetypes[e].mimetype;
			}
		}
	}
	if (!mimetype) 
		mimetype = "application/octet-stream";
	printf("Sending mime-type %s\n", mimetype);
	stringbuf_addstring(headers, "Content-Type: ");
	stringbuf_addstring(headers, mimetype);
	stringbuf_addchar(headers, '\n');
}

//Send a file
static void send_file(const char *name, size_t size, SOCKET sock)
{
	stringbuf *headers = stringbuf_create();
	char sizebuf[128];
	FILE *fptr;
	//Send the headers
	stringbuf_addstring(headers, "HTTP/1.0 200: OK\n");
	sprintf(sizebuf, "Content-Length: %ld\n", size);
	stringbuf_addstring(headers, sizebuf);
	add_mimetype(name, headers);
	stringbuf_addchar(headers, '\n');
	if (!headers->buf) {
		send_message(INTERNAL_SERVER_ERROR, sock);
		return;
	}
	send(sock, headers->buf, headers->len, 0);
	stringbuf_delete(headers);
	//Send the file
	printf("Sending file %s\n", name);
	if ((fptr = fopen(name, "rb")) != NULL) {
		char buf[BUFSIZE];
		const unsigned int size = sizeof(buf);
		int bytes;
		while ((bytes = fread(buf, 1, size, fptr)) > 0) 
			send(sock, buf, bytes, 0);
		fclose(fptr);
	}
	else 
		send_message(NOT_FOUND, sock);
}

//Handle a GET request 
static void handle_get(SOCKET sock, const char *directory, const char *uripath)
{
	char *localpath; //The path we use to read local files and directories
	char *statpath;  //The path we send to _stat (mustn't have a trailing slash) 
	unsigned int dirwanted = 0;
	struct _stat statbuf;
	//Setting the size of localpath as the sum of string lengths of directory string and uripath strings
	localpath = malloc(strlen(directory) + strlen(uripath) + 1);
	if (!localpath) {
		send_message(INTERNAL_SERVER_ERROR, sock);
		return;
	}
	sprintf(localpath, "%s%s", directory, uripath);
	printf("URI path is %s\n", uripath);
	printf("Local path is %s\n", localpath);
	if (strcmp(uripath, "/") == 0)
		strcat(localpath, "index.html");
	statpath = _strdup(localpath);
	if (!statpath) {
		send_message(INTERNAL_SERVER_ERROR, sock);
		return;
	}
	if (statpath[strlen(statpath) - 1] == '/') {
		//Client wants a directory (the trailing / is optional)
		dirwanted = 1;
		//Can't _stat with a trailing 
		statpath[strlen(statpath) - 1] = '\0';
	}
	if (_stat(statpath, &statbuf) == 0) {
		if (statbuf.st_mode & _S_IFDIR) {
			//List the directory
			if (dirPerm == 0)
				send_message(FORBIDDEN, sock);
			else
				send_directory(localpath, uripath, sock);
		}
		else {
			if (!dirwanted) 
				//Send the file
				send_file(localpath, statbuf.st_size, sock);
			else 
				//Directory was requested, but the path is a file
				send_message(NOT_FOUND, sock);
		}
	}
	else 
		//Path not found
		send_message(NOT_FOUND, sock);
	//Cleanup of path strings
	free(localpath);
	free(statpath);
}

//Handle a connection
static UINT handle(LPVOID pParam)
{
	int bytes;
	char buf[BUFSIZE];
	char *method;
	char *uripath; //The path from the request URL, and used to construct URLs
	unsigned int result;
	SOCKET sock = (SOCKET)pParam;
	//Read the request
	bytes = recv(sock, buf, BUFSIZE, 0);
	if (bytes == 0) {
		printf("Client has disconnected\n");
		return 0;
	}
	else if (bytes == SOCKET_ERROR) {
		perror("recv");
		return 0;
	}
	//Parse the request
	result = parse_start(buf, &method, &uripath);
	if (result == 1) {
		send_message(BAD_REQUEST, sock);
		return 0;
	}
	else if (result == 2) {
		send_message(INTERNAL_SERVER_ERROR, sock);
		return 0;
	}
	printf("Method is %s\n", method);
	if (strcmp(method, "GET") == 0) {
		handle_get(sock, directory, uripath);
	}
	else {
		stringbuf *headers = stringbuf_create();
		stringbuf_addstring(headers, "HTTP/1.0 ");
		stringbuf_addstring(headers, METHOD_NOT_ALLOWED);
		stringbuf_addchar(headers, '\n');
		stringbuf_addstring(headers, "Allow: GET\n\n");
		if (!headers->buf) {
			send_message(INTERNAL_SERVER_ERROR, sock);
			return 0;
		}
		send(sock, headers->buf, headers->len, 0);
	}
	closesocket(sock);
	return 0;
}

void ServerThread()
{
	WSADATA wsaData;
	SOCKADDR_IN server;
	uintptr_t thread;
	int port = PORT;
	//Start winsock
	int ret = WSAStartup(0x101, &wsaData); //Use highest version of winsock avalible
	if (ret != 0)
		return;
	//Fill in winsock struct ... 
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT); //Listen on port defined at the top
	//Create our socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
		return;
	//Bind our socket to a port 
	if (bind(sock, (SOCKADDR*)&server, sizeof(server)) != 0)
		return;
	//Listen for a connection  
	if (listen(sock, 5) != 0)
		return;
	struct sockaddr_in their_addr;
	SOCKET newsock;
	size_t size;
	//Loop forever
	while (1) {
		size = sizeof(struct sockaddr);
		//Accept connections
		ZeroMemory(&their_addr, sizeof (struct sockaddr));
		newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
		printf("Client connected\r\n");
		if (newsock == INVALID_SOCKET) 
			perror("accept\n");
		else {
			printf("Got a connection from %s on port %d\n", inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
			handle((LPVOID)newsock);
			//Creating a thread to handle the request
			thread = _beginthread(handle, newsock, NULL);
			if (thread == -1) {
				printf("\nERROR: Cannot create a new thread!\n");
				closesocket(newsock);
			}
		}
	}
}

int main(int argc, char **argv)
{
	printf("\n\nMTWinWebServer Test\n\nUsage : MTWinWebServer [port] ['-d' to disable directory listing, enabled by default]\n\n");
	printf("Press ESCAPE to terminate program\n\n");
	//Check if port was passed as an argument, else use default
	if (argc == 2) {
		printf("\nUsing port %s.\n\n", argv[1]);
		PORT = atoi(argv[1]);
	}
	else if (argc == 3)
	{
		if (strcmp(argv[2], "-d") == 0)
		{
			printf("\nUsing port %s and directory listing is disabled.\n\n", argv[1]);
			dirPerm = 0;
		}
		else
			printf("\nUsing port %s.\n\n", argv[1]);
		PORT = atoi(argv[1]);
	}
	else
		printf("\nUsing default port 8080 as no port number was passed as argument.\n\n");
	//Listening for connections
	ServerThread();
	//Loop till esc key is pressed
	while (_getch() != 27);
	//Close socket
	closesocket(sock);
	//Shutdown winsock
	WSACleanup();
	//Exit
	return 0;
}