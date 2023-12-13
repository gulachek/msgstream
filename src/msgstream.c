#include "msgstream.h"
#include <assert.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>

static void fperror(FILE *err, const char *s) {
  if (!err)
    return;

  char *msg = strerror(errno);
  if (s)
    fprintf(err, "%s: %s\n", s, msg);
  else
    fprintf(err, "%s\n", msg);
}

msgstream_size msgstream_header_size(size_t buf_size, FILE *err) {
  if (buf_size < 1) {
    if (err)
      fprintf(err, "Message buffer must be at least 1 byte large\n");
    return MSGSTREAM_ERR;
  }

  size_t nbytes = 0;
  while (buf_size > 0) {
    buf_size /= 256;
    nbytes += 1;
  }

  size_t out = 1 + nbytes;
  if (out > MSGSTREAM_HEADER_BUF_SIZE) {
    if (err)
      fprintf(
          err,
          "Message header would be too big for a message buffer of size %lu\n",
          buf_size);
    return MSGSTREAM_ERR;
  }

  return out;
}

msgstream_size msgstream_encode_header(void *header_buf, size_t header_buf_size,
                                       size_t msg_buf_size,
                                       msgstream_size msg_size, FILE *err) {
  if (msg_size > msg_buf_size) {
    if (err)
      fprintf(err,
              "Buffer size '%lu' is not large enough to fit message of size "
              "'%lld'\n",
              msg_buf_size, msg_size);
    return MSGSTREAM_ERR;
  }

  size_t nheader = msgstream_header_size(msg_buf_size, err);
  if (nheader < 0)
    return MSGSTREAM_ERR;

  if (nheader > header_buf_size) {
    if (err)
      fprintf(err,
              "msgstream header buf of size '%lu' is too small to fit header "
              "of size '%lu'\n",
              header_buf_size, nheader);
    return MSGSTREAM_ERR;
  }

  uint8_t *buf = (uint8_t *)header_buf;

  // first part is header size
  buf[0] = (uint8_t)nheader;

  // next part is little endian message size
  size_t i = 1;
  while (msg_size > 0) {
    buf[i] = msg_size % 256;
    msg_size /= 256;
    i += 1;
  }

  for (; i < nheader; ++i)
    buf[i] = 0;

  return nheader;
}

static msgstream_size readn(msgstream_fd fd, void *buf, msgstream_size nbytes,
                            FILE *err) {
  int expect_eof = 1;
  msgstream_size nread = 0;
  while (nbytes > nread) {
    ssize_t n = read(fd, buf + nread, nbytes - nread);
    if (n == -1) {
      fperror(err, "read");
      return MSGSTREAM_ERR;
    }

    if (n == 0) {
      if (expect_eof) {
        return MSGSTREAM_EOF;
      } else {
        if (err)
          fprintf(err, "Unexpected eof\n");
        return MSGSTREAM_ERR;
      }
    }

    nread += n;
    expect_eof = 0;
  }

  return nbytes;
}

msgstream_size msgstream_decode_header(const void *header_buf,
                                       size_t header_size, FILE *err) {
  const uint8_t *buf = (const uint8_t *)header_buf;

  if (buf[0] != header_size) {
    if (err)
      fprintf(err,
              "Received unexpected msgstream header size. Expected '%lu' but "
              "received '%d'\n",
              header_size, buf[0]);
    return MSGSTREAM_ERR;
  }

  msgstream_size msg_size = 0;
  msgstream_size mult = 1;
  for (size_t i = 1; i < header_size; ++i) {
    msg_size += mult * buf[i];
    mult *= 256;
  }

  return msg_size;
}

int msgstream_fd_send(int fd, const void *buf, size_t buf_size,
                      size_t msg_size) {
  assert(buf_size > 0);

  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  msgstream_size header_size = msgstream_encode_header(
      header_buf, sizeof(header_buf), buf_size, msg_size, NULL);

  if (header_size < 0) {
    /*
if (err)
fprintf(err, "Error encoding msgstream header\n");
            */
    return MSGSTREAM_ERR;
  }

  // TODO - one system call?
  if (write(fd, header_buf, header_size) == -1) {
    // fperror(err, "error writing msgstream header");
    return MSGSTREAM_ERR;
  }

  if (write(fd, buf, msg_size) == -1) {
    // fperror(err, "error writing msgstream body");
    return MSGSTREAM_ERR;
  }

  return MSGSTREAM_OK;
}

int msgstream_fd_recv(int fd, void *buf, size_t buf_size, size_t *msg_size) {
  assert(msg_size);
  *msg_size = 0;

  msgstream_size nheader = msgstream_header_size(buf_size, NULL);
  if (nheader < 0)
    return MSGSTREAM_ERR;

  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  msgstream_size nheader_ret = readn(fd, header_buf, nheader, NULL);
  if (nheader_ret < 0) {
    if (nheader_ret == MSGSTREAM_EOF)
      return MSGSTREAM_EOF;

    return MSGSTREAM_ERR;
  }

  size_t msize = msgstream_decode_header(header_buf, nheader, NULL);
  if (msize <= 0)
    return msize;

  msgstream_size body_size = readn(fd, buf, msize, NULL);
  if (body_size < 0) {
    if (body_size == MSGSTREAM_EOF)
      // fprintf(err, "Unexpected eof\n");

      // fprintf(err, "Failed to read msgstream body\n");
      return MSGSTREAM_ERR;
  }

  *msg_size = msize;
  return MSGSTREAM_OK;
}
