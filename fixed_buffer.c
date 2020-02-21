#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

/**
 *  Read to the end of the file and count the bytes read.
 */
static int bytes_remaining(int fd, size_t *file_size)
{
	int rc = 0;
	size_t bytes_read;
	char *temp_buf = malloc(64);

	if (temp_buf == NULL)
		return -ENOMEM;

	while (1) {
		ssize_t count = read(fd, temp_buf, 64);

		if (count == 0) {
			*file_size = bytes_read;
			break;
		}

		if (count < 0) {
			rc = -errno;
			break;
		}

		bytes_read += count;
	}

	free(temp_buf);
	return rc;
}

/**
 *  Copy file to a buffer and write the number of bytes copied 
 */
static int copy_file_fixed(char *path, char *buf, size_t *buflen)
{
	int rc = 0;
	int fd;
	size_t bytes_read = 0;
	size_t max_read = *buflen - 1;
	int read_all = 0;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -errno;

	while (bytes_read < max_read) {
		ssize_t count = read(fd,
				     buf + bytes_read,
				     max_read - bytes_read);

		if (count == 0) {
			*buflen = bytes_read + 1;
			buf[bytes_read] = '\0';
			read_all = 1;
			break;
		}

		if (count < 0) {
			rc = -errno;
			goto err;
		}

		bytes_read += count;
	}

	/* buf full except last byte */
	if (!read_all) {
		size_t remaining;

		buf[max_read] = '\0';

		rc = bytes_remaining(fd, &remaining);
		if (rc != 0) {
			rc = -errno;
			goto err;
		}

		*buflen = bytes_read + remaining + 1;

		/* file was not exactly (*buflen - 1) bytes */
		if (remaining != 0)
			rc = -EOVERFLOW;
	}
err:
	close(fd);

	return rc;
}


int main(int argc, char *argv[])
{

	int rc = 0;
	if (argc < 3) {
		printf("need args: path size\n");
		return 0;
	}

	size_t buflen = (size_t)atoi(argv[2]);
	size_t output = buflen;
	char *buf = malloc(buflen);
	char *path = argv[1];

	rc = copy_file_fixed(path, buf, &output);

	printf("data=\n%s\n", buf);
	printf("size=%lu\n", output);
	printf("rc=%s\n", strerror(-rc));


	
	return rc;
}

