// gcc -o http-server http-server.c

// documentation: man 7 ip

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main () {
	int sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (8080);
	addr.sin_addr.s_addr = htonl (INADDR_ANY);
	bind (sock, (struct sockaddr*)&addr, sizeof(addr));
	listen (sock, 1);
	while (1) {
		int fd = accept (sock, NULL, NULL);
		char buf[1024];
		ssize_t size = read (fd, buf, 1024);
		fwrite (buf, 1, size, stdout);
		fputc ('\n', stdout);
		const char *response = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello, World!</h1>";
		write (fd, response, strlen (response));
		close (fd);
	}
	close (sock);
	return 0;
}
