#ifndef MSGSTREAM_H
#define MSGSTREAM_H

#include <stdint.h>
#include <stdio.h>

#include "msgstream/errc.h"

#ifdef MSGSTREAM_IS_COMPILING
#define MSGSTREAM_API EXPORT
#else
#define MSGSTREAM_API
#endif

typedef int msgstream_fd;
typedef int64_t msgstream_size;

#define MSGSTREAM_HEADER_BUF_SIZE 9

#define MSGSTREAM_ERR -1
#define MSGSTREAM_EOF -2

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determine how many bytes a message header will be given the message buffer
 * size.
 * @param buf_size The size of the message buffer in bytes
 * @param err An optional stream to write error messages to
 * @return The number of bytes to read for the associated message's header
 */
MSGSTREAM_API msgstream_size msgstream_header_size(size_t buf_size, FILE *err);

/**
 * Encode a message header into a buffer
 * @param header_buf The buffer in which to encode the header
 * @param header_buf_size The size of header_buf in bytes
 * @param msg_buf_size The size of the buffer receiving the message
 * @param msg_size The size of the message whose header is being encoded
 * @param err Optional stream to write error messages to
 * @return The size of the encoded header in bytes (msgstream_header_size)
 */
MSGSTREAM_API msgstream_size msgstream_encode_header(void *header_buf,
                                                     size_t header_buf_size,
                                                     size_t msg_buf_size,
                                                     msgstream_size msg_size,
                                                     FILE *err);

/**
 * Decode a message header into a message size
 * @param header_buf The buffer holding the header to decode
 * @param header_size The size of header_in bytes obtained from
 * msgstream_header_size
 * @param err Optional stream to write error messages to
 * @return The size of the message body associated with the header
 */
MSGSTREAM_API msgstream_size msgstream_decode_header(const void *header_buf,
                                                     size_t header_size,
                                                     FILE *err);

/**
 * Send a message over a file descriptor
 * @param fd The file decriptor to write the message to
 * @param buf A buffer holding the message to be sent
 * @param buf_size The size of the buffer in bytes
 * @param msg_size The size of the message in bytes (<= buf_size)
 * @param err An optional stream to write error messages to
 * @return msg_size on success and a negative value on failure
 */
MSGSTREAM_API msgstream_size msgstream_send(msgstream_fd fd, const void *buf,
                                            msgstream_size buf_size,
                                            msgstream_size msg_size, FILE *err);

/**
 * Receive a message over a file descriptor
 * @param fd The file decriptor to read the message from
 * @param buf A buffer to hold the received message
 * @param buf_size The size of the buffer in bytes
 * @param msg_size The size of the received message will be stored here
 * @return An error code
 */
MSGSTREAM_API int msgstream_fd_recv(int fd, void *buf, size_t buf_size,
                                    size_t *msg_size);

/**
 * Return a string that describes the given error code
 * @param ec The error code
 * @return A string that describes the error code
 */
MSGSTREAM_API const char *msgstream_errstr(int ec);

/**
 * Return a string that corresponds to the given error code name
 * @param ec The error code
 * @return The error code's name (like "MSGSTREAM_OK")
 */
MSGSTREAM_API const char *msgstream_errname(int ec);

#ifdef __cplusplus
}
#endif

#endif
