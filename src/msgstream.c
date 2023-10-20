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

static size_t header_size(msgstream_size buf_size) {
  size_t nbytes = 0;
  while (buf_size > 0) {
    buf_size /= 256;
    nbytes += 1;
  }

  return 1 + nbytes;
}

static msgstream_size write_header(msgstream_fd fd, msgstream_size buf_size,
                                   msgstream_size msg_size, FILE *err) {
  if (msg_size > buf_size) {
    if (err)
      fprintf(err,
              "Buffer size '%lld' is not large enough to fit message of size "
              "'%lld'\n",
              buf_size, msg_size);
    return -1;
  }

  uint8_t buf[256];
  size_t nheader = header_size(buf_size);
  if (nheader > 0xff) {
    if (err)
      fprintf(
          err,
          "msgstream header size of '%lu' is too big. must fit in one byte\n",
          nheader);
    return -1;
  }

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

  if (write(fd, buf, nheader) == -1) {
    fperror(err, "error writing msgstream header");
    return -1;
  }

  return nheader;
}

static msgstream_size readn(msgstream_fd fd, void *buf, msgstream_size nbytes,
                            FILE *err) {
  msgstream_size nread = 0;
  while (nbytes > nread) {
    ssize_t n = read(fd, buf + nread, nbytes - nread);
    if (n == -1) {
      fperror(err, "read");
      return -1;
    }

    if (n == 0) {
      if (err)
        fprintf(err, "Unexpected eof\n");
      return -1;
    }

    nread += n;
  }

  return nbytes;
}

static msgstream_size read_header(msgstream_fd fd, msgstream_size buf_size,
                                  FILE *err) {
  size_t nheader = header_size(buf_size);
  if (nheader > 0xff) {
    if (err)
      fprintf(
          err,
          "msgstream header size of '%lu' is too big. must fit in one byte\n",
          nheader);
    return -1;
  }

  const char *err_read = "error reading msgstream header";
  const char *err_eof = "Unexpected eof while reading msgstream header";

  // read once to validate header size match
  uint8_t buf[256];
  int nread = read(fd, buf, nheader);
  if (nread == -1) {
    fperror(err, err_read);
    return -1;
  }

  if (nread == 0) {
    if (err)
      fprintf(err, "%s\n", err_eof);
    return -1;
  }

  if (buf[0] != nheader) {
    if (err)
      fprintf(err,
              "Received unexpected msgstream header size. Expected '%lu' but "
              "received '%d'\n",
              nheader, buf[0]);
    return -1;
  }

  if (readn(fd, buf + nread, nheader - nread, err) == -1) {
    if (err)
      fprintf(err, "Failed to complete reading the msgstream header\n");
    return -1;
  }

  msgstream_size msg_size = 0;
  msgstream_size mult = 1;
  for (size_t i = 1; i < nheader; ++i) {
    msg_size += mult * buf[i];
    mult *= 256;
  }

  return msg_size;
}

msgstream_size msgstream_write(msgstream_fd fd, void *buf,
                               msgstream_size buf_size, msgstream_size msg_size,
                               FILE *err) {
  assert(buf_size > 0);

  if (write_header(fd, buf_size, msg_size, err) == -1) {
    return -1;
  }

  if (write(fd, buf, msg_size) == -1) {
    fperror(err, "error writing msgstream body");
    return -1;
  }

  return msg_size;
}

msgstream_size msgstream_read(msgstream_fd fd, void *buf,
                              msgstream_size buf_size, FILE *err) {
  assert(buf_size > 0);

  msgstream_size msg_size = read_header(fd, buf_size, err);
  if (msg_size == -1) {
    return -1;
  }

  if (msg_size == 0)
    return 0;

  if (readn(fd, buf, msg_size, err) == -1) {
    if (err)
      fprintf(err, "Failed to read msgstream body\n");
    return -1;
  }

  return msg_size;
}
