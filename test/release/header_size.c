#include <msgstream.h>
#include <stdio.h>

int main() {
  size_t msg_sz = 2048, sz;
  if (msgstream_header_size(msg_sz, &sz) != MSGSTREAM_OK) {
    return 1;
  }

  printf("Message of size %lu has header of size %lu\n", msg_sz, sz);
  return 0;
}
