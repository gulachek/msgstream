#ifndef MSGSTREAM_H
#define MSGSTREAM_H

#include <stdint.h>
#include <stdio.h>

typedef int msgstream_fd;
typedef int64_t msgstream_size;

msgstream_size msgstream_write(msgstream_fd fd, void *buf,
                               msgstream_size buf_size, msgstream_size msg_size,
                               FILE *err);

msgstream_size msgstream_read(msgstream_fd, void *buf, msgstream_size buf_size,
                              FILE *err);

#endif
