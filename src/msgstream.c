#include "msgstream.h"
#include <assert.h>
#include <stdlib.h>
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

enum msg_read_stage { HEADER, MSG };

struct msgstream_incremental_reader_ {
  uint8_t hdr_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size;

  uint8_t *buf;
  size_t buf_size;
  size_t msg_size;

  enum msg_read_stage stage;
  size_t nread;
};

msgstream_incremental_reader
msgstream_incremental_reader_alloc(void *buf, size_t buf_size) {
  struct msgstream_incremental_reader_ *reader =
      malloc(sizeof(struct msgstream_incremental_reader_));

  if (!reader)
    return NULL;

  int ec = msgstream_header_size(buf_size, &reader->hdr_size);
  if (ec != MSGSTREAM_OK) {
    free(reader);
    return NULL;
  }
  memset(reader->hdr_buf, 0, MSGSTREAM_HEADER_BUF_SIZE);

  reader->buf = buf;
  reader->buf_size = buf_size;
  reader->msg_size = 0;

  reader->stage = HEADER;
  reader->nread = 0;

  return reader;
}

void msgstream_incremental_reader_free(msgstream_incremental_reader reader) {
  if (reader)
    free(reader);
}

static int incremental_readn(int fd, size_t n, uint8_t *buf, size_t *pnread) {
  size_t nread = *pnread;
  if (nread >= n) {
    return MSGSTREAM_OK;
  }

  size_t nleft = n - nread;
  int nbytes = read(fd, buf + nread, nleft);
  if (nbytes < 0) {
    if (errno == EAGAIN)
      return MSGSTREAM_OK;
    else
      return MSGSTREAM_SYS_READ_ERR;
  } else if (nbytes == 0) {
    if (nread == 0) {
      return MSGSTREAM_EOF;
    } else {
      return MSGSTREAM_TRUNC;
    }
  } else {
    *pnread = nread + nbytes;
    return MSGSTREAM_OK;
  }
}

int msgstream_fd_incremental_recv(int fd, msgstream_incremental_reader reader,
                                  int *is_complete, size_t *pmsg_size) {
  if (!(is_complete && reader && pmsg_size))
    return MSGSTREAM_NULL_ARG;

  *is_complete = 0;
  *pmsg_size = 0;

  if (reader->stage == HEADER) {
    int ec = incremental_readn(fd, reader->hdr_size, reader->hdr_buf,
                               &reader->nread);

    if (ec == MSGSTREAM_OK) {
      if (reader->nread == reader->hdr_size) {
        ec = msgstream_decode_header(reader->hdr_buf, reader->hdr_size,
                                     &reader->msg_size);
        if (ec != MSGSTREAM_OK)
          return ec;

        reader->stage = MSG;
        reader->nread = 0;

        if (reader->msg_size == 0) {
          *is_complete = 1;
          *pmsg_size = 0;
          reader->stage = HEADER;
          return MSGSTREAM_OK;
        }
      } else if (reader->nread > 0) {
        if (reader->hdr_buf[0] != reader->hdr_size) {
          ec = MSGSTREAM_HDR_SYNC;
        }
      }
    }

    return ec;
  } else {
    int ec =
        incremental_readn(fd, reader->msg_size, reader->buf, &reader->nread);

    if (reader->nread == reader->msg_size) {
      *is_complete = 1;
      *pmsg_size = reader->msg_size;

      // reset to read next message
      reader->stage = HEADER;
      reader->nread = 0;
    }

    return ec;
  }
}
