/*
 * Simple file client.
 *
 * Operating Systems, 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.h"
#include "debug.h"
#include "sock-util.h"
#include "w_epoll.h"

#define PORT			42424
#define BUFSIZE			10
#define NAME_MAX_LEN		256

static void usage() {
	fprintf(stdout, "Usage: "
			"client <remote_filename> <local_filename>\n");
}

/*
 * "upgraded" read routine
 */

static ssize_t xread(int fd, void *buffer, size_t len)
{
	ssize_t ret;
	ssize_t n;

	n = 0;
	while (n < (ssize_t) len) {
		ret = read(fd, (char *) buffer + n, len - n);
		if (ret < 0)
			return -1;
		if (ret == 0)
			break;
		n += ret;
	}

	return n;
}

/*
 * "upgraded" write routine
 */

static ssize_t xwrite(int fd, const void *buffer, size_t len)
{
	ssize_t ret;
	ssize_t n;

	n = 0;
	while (n < (ssize_t) len) {
		ret = write(fd, (const char *) buffer + n, len - n);
		if (ret < 0)
			return -1;
		if (ret == 0)
			break;
		n += ret;
	}

	return n;
}

/*
 * "upgraded" recv routine
 */

static ssize_t xrecv(int s, void *buffer, size_t len)
{
	return xread(s, buffer, len);
}

/*
 * "upgraded" send routine
 */

static ssize_t xsend(int s, const void *buffer, size_t len)
{
	return xwrite(s, buffer, len);
}

static int send_filename(int s, char *buffer, size_t len)
{
	ssize_t n;

	n = xsend(s, buffer, len);

	return n < 0 ? -1 : 0;
}

static int receive_file(int s, char *dname)
{
	ssize_t n, n2;
	int fd;
	int err = 0;
	char buffer[BUFSIZE];

	fd = open(dname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		ERR("open");
		goto bad_file;
	}

	while (1) {
		n = xrecv(s, buffer, BUFSIZE);
		if (n < 0) {
			ERR("recv");
			goto bad_recv;
		}

		if (n == 0)
			break;

		n2 = xwrite(fd, buffer, n);
		if (n2 < 0) {
			ERR("write");
			goto bad_write;
		}

	}

	close(fd);

out:
	return err;

bad_recv:
bad_write:
	close(fd);
bad_file:
	err = -1;
	goto out;

}

int main(int argc, char **argv)
{
	int sockfd;		/* client socket file descriptor */
	int rc, err = 0;
	char fname[NAME_MAX_LEN];	/* remote file name */
	char dname[NAME_MAX_LEN];	/* local file name */

	if (argc != 3) {
		usage();
		exit(1);
	}


	memset(fname, 0, NAME_MAX_LEN);
	memset(dname, 0, NAME_MAX_LEN);
	strncpy(fname, argv[1], NAME_MAX_LEN);
	strncpy(dname, argv[2], NAME_MAX_LEN);

	/* Connect to server */
	sockfd = tcp_connect_to_server("localhost", PORT);
	DIE(sockfd < 0, "tcp_connect_to_server");

	rc = send_filename(sockfd, fname, NAME_MAX_LEN);
	if (rc < 0) {
		err = -1;
		goto done;
	}

	rc = receive_file(sockfd, dname);
	if (rc < 0) {
		err = -2;
		goto done;
	}

done:
	tcp_close_connection(sockfd);
	return err;
}
