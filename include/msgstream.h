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

/**
 * Message headers will never need more than this many bytes to be allocated
 */
#define MSGSTREAM_HEADER_BUF_SIZE 9

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determine how many bytes a message header will be given the message buffer
 * size.
 * @param[in] msg_buf_size The size of the message buffer in bytes
 * @param[out] hdr_size The size of the header
 * @return An error code
 */
MSGSTREAM_API int msgstream_header_size(size_t msg_buf_size, size_t *hdr_size);

/**
 * Encode a message header into a buffer
 * @param[in] msg_size The size of the message whose header is being encoded
 * @param[in] hdr_size The size of the encoded header obtained from
 * msgstream_header_size
 * @param[out] hdr_buf The buffer in which to encode the header
 * @return An error code
 */
MSGSTREAM_API int msgstream_encode_header(size_t msg_size, size_t hdr_size,
                                          void *hdr_buf);

/**
 * Decode a message header into a message size
 * @param[in] header_buf The buffer holding the header to decode
 * @param[in] header_size The size of header_in bytes obtained from
 * msgstream_header_size
 * @param[out] msg_size The size of the message payload for the decoded header
 * @return An error code
 */
MSGSTREAM_API int msgstream_decode_header(const void *header_buf,
                                          size_t header_size, size_t *msg_size);

/**
 * Send a message over a file descriptor
 * @param[in] fd The file decriptor to write the message to
 * @param[in] buf A buffer holding the message to be sent
 * @param[in] buf_size The size of the buffer in bytes
 * @param[in] msg_size The size of the message in bytes (<= buf_size)
 * @return An error code
 */
MSGSTREAM_API int msgstream_fd_send(int fd, const void *buf, size_t buf_size,
                                    size_t msg_size);

/**
 * Receive a message over a file descriptor
 * @param[in] fd The file decriptor to read the message from
 * @param[in] buf A buffer to hold the received message
 * @param[in] buf_size The size of the buffer in bytes
 * @param[out] msg_size The size of the received message
 * @return An error code
 */
MSGSTREAM_API int msgstream_fd_recv(int fd, void *buf, size_t buf_size,
                                    size_t *msg_size);

/**
 * Return a string that describes the given error code
 * @param[in] ec The error code
 * @return A string that describes the error code
 */
MSGSTREAM_API const char *msgstream_errstr(int ec);

/**
 * Return a string that corresponds to the given error code name
 * @param[in] ec The error code
 * @return The error code's name (like "MSGSTREAM_OK")
 */
MSGSTREAM_API const char *msgstream_errname(int ec);

#ifdef __cplusplus
}
#endif

#endif
