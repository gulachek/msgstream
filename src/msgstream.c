#include "msgstream.h"
#include <assert.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>

int msgstream_header_size(size_t buf_size, size_t *hdr_size) {
  if (!hdr_size)
    return MSGSTREAM_NULL_ARG;
  *hdr_size = 0;

  if (buf_size < 1)
    return MSGSTREAM_SMALL_BUF;

  size_t nbytes = 0;
  while (buf_size > 0) {
    buf_size /= 256;
    nbytes += 1;
  }

  size_t out = 1 + nbytes;
  if (out > MSGSTREAM_HEADER_BUF_SIZE)
    return MSGSTREAM_BIG_HDR;

  *hdr_size = out;
  return MSGSTREAM_OK;
}

int msgstream_encode_header(size_t msg_size, size_t hdr_size, void *hdr_buf) {
  if (!hdr_buf)
    return MSGSTREAM_NULL_ARG;

  if (hdr_size > MSGSTREAM_HEADER_BUF_SIZE)
    return MSGSTREAM_BIG_HDR;

  if (hdr_size < 1)
    return MSGSTREAM_SMALL_HDR;

  uint8_t *buf = (uint8_t *)hdr_buf;

  // first part is header size
  buf[0] = (uint8_t)hdr_size;

  // next part is little endian message size
  size_t i = 1;
  while (msg_size > 0) {
    if (i >= hdr_size)
      return MSGSTREAM_BIG_MSG;

    buf[i] = msg_size % 256;
    msg_size /= 256;
    i += 1;
  }

  for (; i < hdr_size; ++i)
    buf[i] = 0;

  return MSGSTREAM_OK;
}

static int readn(int fd, void *buf, size_t nbytes) {
  int expect_eof = 1;
  size_t nread = 0;
  while (nbytes > nread) {
    ssize_t n = read(fd, buf + nread, nbytes - nread);
    if (n == -1)
      return MSGSTREAM_SYS_READ_ERR;

    if (n == 0)
      return expect_eof ? MSGSTREAM_EOF : MSGSTREAM_TRUNC;

    nread += n;
    expect_eof = 0;
  }

  return MSGSTREAM_OK;
}

int msgstream_decode_header(const void *header_buf, size_t header_size,
                            size_t *msg_size) {
  if (!(header_buf && msg_size))
    return MSGSTREAM_NULL_ARG;

  if (header_size < 1)
    return MSGSTREAM_SMALL_HDR;

  if (header_size > MSGSTREAM_HEADER_BUF_SIZE)
    return MSGSTREAM_BIG_HDR;

  const uint8_t *buf = (const uint8_t *)header_buf;

  if (buf[0] != header_size)
    return MSGSTREAM_HDR_SYNC;

  size_t msize = 0;
  size_t mult = 1;
  for (size_t i = 1; i < header_size; ++i) {
    msize += mult * buf[i];
    mult *= 256;
  }

  *msg_size = msize;
  return MSGSTREAM_OK;
}

int msgstream_fd_send(int fd, const void *buf, size_t buf_size,
                      size_t msg_size) {
  int ec;
  size_t hdr_size;
  if ((ec = msgstream_header_size(buf_size, &hdr_size)))
    return ec;

  uint8_t hdr_buf[MSGSTREAM_HEADER_BUF_SIZE];
  if ((ec = msgstream_encode_header(msg_size, hdr_size, hdr_buf)))
    return ec;

  // TODO - one system call?
  if (write(fd, hdr_buf, hdr_size) == -1)
    return MSGSTREAM_SYS_WRITE_ERR;

  if (write(fd, buf, msg_size) == -1)
    return MSGSTREAM_SYS_WRITE_ERR;

  return MSGSTREAM_OK;
}

int msgstream_fd_recv(int fd, void *buf, size_t buf_size, size_t *msg_size) {
  if (!msg_size)
    return MSGSTREAM_NULL_ARG;
  *msg_size = 0;

  size_t hdr_size;
  int ec;
  if ((ec = msgstream_header_size(buf_size, &hdr_size)))
    return ec;

  uint8_t hdr_buf[MSGSTREAM_HEADER_BUF_SIZE];
  if ((ec = readn(fd, hdr_buf, hdr_size)))
    return ec;

  size_t msize;
  if ((ec = msgstream_decode_header(hdr_buf, hdr_size, &msize)))
    return ec;

  if ((ec = readn(fd, buf, msize))) {
    if (ec == MSGSTREAM_EOF)
      return MSGSTREAM_TRUNC;

    return ec;
  }

  *msg_size = msize;
  return MSGSTREAM_OK;
}
