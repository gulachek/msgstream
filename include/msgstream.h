#ifndef MSGSTREAM_H
#define MSGSTREAM_H

#include <stdint.h>
#include <stdio.h>

#ifdef MSGSTREAM_IS_COMPILING
#define MSGSTREAM_API EXPORT
#else
#define MSGSTREAM_API
#endif

typedef int msgstream_fd;
typedef int64_t msgstream_size;

#define MSGSTREAM_ERR -1
#define MSGSTREAM_EOF -2

#ifdef __cplusplus
extern "C" {
#endif

MSGSTREAM_API msgstream_size msgstream_send(msgstream_fd fd, void *buf,
                                            msgstream_size buf_size,
                                            msgstream_size msg_size, FILE *err);

MSGSTREAM_API msgstream_size msgstream_recv(msgstream_fd, void *buf,
                                            msgstream_size buf_size, FILE *err);

#ifdef __cplusplus
}
#endif

#endif
