#define BOOST_TEST_MODULE example
#include <boost/test/unit_test.hpp>

#include "msgstream.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string_view>
#include <thread>

using std::size_t;
using std::uint8_t;

BOOST_AUTO_TEST_CASE(OkIsFalsy) { BOOST_TEST(!MSGSTREAM_OK); }

BOOST_AUTO_TEST_CASE(ErrNameOk) {
  std::string_view nm{msgstream_errname(MSGSTREAM_OK)};
  BOOST_TEST(nm == "MSGSTREAM_OK");
}

BOOST_AUTO_TEST_CASE(ErrStrOk) {
  std::string_view str{msgstream_errstr(MSGSTREAM_OK)};
  BOOST_TEST(str == "no error detected");
}

struct f {
  f() {
    int fds[2];
    if (pipe(fds) == -1) {
      perror("pipe");
      BOOST_FAIL("Failed to allocate pipe");
    }

    read_ = fds[0];
    write_ = fds[1];
  }

  ~f() {
    close(read_);
    close(write_);
  }

  int read_;
  int write_;
};

BOOST_FIXTURE_TEST_CASE(TransferSingleByte, f) {
  int8_t b42 = 42, recv = 0;
  auto ec = msgstream_fd_send(write_, &b42, sizeof(b42), 1);
  BOOST_TEST(!ec);

  size_t size = 0;
  ec = msgstream_fd_recv(read_, &recv, sizeof(recv), &size);
  BOOST_TEST(!ec);

  BOOST_TEST(size == 1);
  BOOST_TEST(recv == 42);
}

BOOST_FIXTURE_TEST_CASE(EofIsDistinctError, f) {
  int8_t recv = 0;
  close(write_);
  size_t size = 1;
  auto ec = msgstream_fd_recv(read_, &recv, sizeof(recv), &size);

  BOOST_TEST(ec == MSGSTREAM_EOF);
  BOOST_TEST(size == 0);
}

BOOST_FIXTURE_TEST_CASE(TransferEmptyMsg, f) {
  int8_t b42 = 42, recv = 96;
  auto ec = msgstream_fd_send(write_, &b42, sizeof(b42), 0);
  BOOST_TEST(!ec);

  size_t size = 1;
  ec = msgstream_fd_recv(read_, &recv, sizeof(recv), &size);
  BOOST_TEST(!ec);
  BOOST_TEST(size == 0);

  BOOST_TEST(recv == 96); // no change
}

BOOST_FIXTURE_TEST_CASE(TransferStringHello, f) {
  char hello[9] = "hello", recv[9] = {};
  size_t len = strlen(hello);

  auto ec = msgstream_fd_send(write_, hello, sizeof(hello), len);
  BOOST_TEST(!ec);

  size_t size = 0;
  ec = msgstream_fd_recv(read_, recv, sizeof(recv), &size);
  BOOST_TEST(!ec);
  BOOST_TEST(size == len);

  std::string_view recv_sv{recv, recv + size};
  BOOST_TEST(recv_sv == "hello");
}

#undef HUGE
#define HUGE 0x12345

BOOST_FIXTURE_TEST_CASE(TransferHugeMessage, f) {
  uint8_t huge[HUGE], recv[HUGE] = {};

  for (size_t i = 0; i < HUGE; ++i)
    huge[i] = i % 256;

  int sret = MSGSTREAM_EOF;
  std::thread th{[&] { sret = msgstream_fd_send(write_, huge, HUGE, HUGE); }};

  size_t size;
  auto rret = msgstream_fd_recv(read_, recv, HUGE, &size);
  th.join();

  BOOST_TEST(rret == MSGSTREAM_OK);
  BOOST_TEST(sret == MSGSTREAM_OK);
  BOOST_TEST(size == HUGE);

  for (size_t i = 0; i < HUGE; ++i) {
    if (recv[i] != (i % 256)) {
      std::ostringstream os;
      os << "Failed to read byte at index " << i << ". Expected " << (i % 256)
         << " but received " << recv[i];

      BOOST_FAIL(os.str());
    }
  }
}

BOOST_AUTO_TEST_SUITE(header_size)

#define EXPAND(X) X

#define DO_TEST(BUF_SZ, RET)                                                   \
  {                                                                            \
    size_t b##BUF_SZ;                                                          \
    int ec = msgstream_header_size(BUF_SZ, &EXPAND(b##BUF_SZ));                \
    BOOST_TEST(!ec);                                                           \
    BOOST_TEST(EXPAND(b##BUF_SZ) == RET);                                      \
  }

BOOST_AUTO_TEST_CASE(EmptyBufferIsMeaninglessAndErrors) {
  size_t n = 1;
  int ec = msgstream_header_size(0, &n);
  BOOST_TEST(ec == MSGSTREAM_SMALL_BUF);
}

BOOST_AUTO_TEST_CASE(TwoByteHeader){DO_TEST(0x1, 2) DO_TEST(0xff, 2)}

BOOST_AUTO_TEST_CASE(ThreeByteHeader){DO_TEST(0x100, 3) DO_TEST(0xffff, 3)}

BOOST_AUTO_TEST_CASE(FourByteHeader){DO_TEST(0x10000, 4) DO_TEST(0xffffff, 4)}

BOOST_AUTO_TEST_CASE(FiveByteHeader){DO_TEST(0x1000000, 5)
                                         DO_TEST(0xffffffff, 5)}

BOOST_AUTO_TEST_CASE(SixByteHeader){DO_TEST(0x100000000, 6)
                                        DO_TEST(0xffffffffff, 6)}

BOOST_AUTO_TEST_CASE(SevenByteHeader){DO_TEST(0x10000000000, 7)
                                          DO_TEST(0xffffffffffff, 7)}

BOOST_AUTO_TEST_CASE(EightByteHeader){DO_TEST(0x1000000000000, 8)
                                          DO_TEST(0xffffffffffffff, 8)}

BOOST_AUTO_TEST_CASE(NineByteHeader){DO_TEST(0x100000000000000, 9)
                                         DO_TEST(0xffffffffffffffff, 9)}

#undef EXPAND
#undef DO_TEST

BOOST_AUTO_TEST_SUITE_END() // header_size

    BOOST_AUTO_TEST_SUITE(EncodeHeader)

        BOOST_AUTO_TEST_CASE(EmptyBufferIsMeaninglessAndErrors) {
  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ec = msgstream_encode_header(0, 0, header_buf);
  BOOST_TEST(ec == MSGSTREAM_SMALL_HDR);
}

BOOST_AUTO_TEST_CASE(MsgSizeLargerThanBufSizeIsError) {
  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ec = msgstream_encode_header(0xffff, 2, header_buf);
  BOOST_TEST(ec == MSGSTREAM_BIG_MSG);
}

BOOST_AUTO_TEST_CASE(EncodesSingleByteMessageHeaderAsTwoBytes) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size = 2;
  auto ec = msgstream_encode_header(1, hdr_size, header_buf);
  BOOST_TEST(!ec);

  std::string_view header{header_buf, header_buf + hdr_size};
  BOOST_TEST(header == "\x02\x01");
}

BOOST_AUTO_TEST_CASE(EncodesMultiByteHeaderWithLittleEndianMsgSize) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size = 5;
  auto ec = msgstream_encode_header(0x04030201, hdr_size, header_buf);
  BOOST_TEST(!ec);

  std::string_view header{header_buf, header_buf + hdr_size};
  BOOST_TEST(header == "\x05\x01\x02\x03\x04");
}

BOOST_AUTO_TEST_CASE(EncodesNineByteHeaderWithLittleEndianMsgSize) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size = 9;
  auto ec = msgstream_encode_header(0x0807060504030201, hdr_size, header_buf);
  BOOST_TEST(!ec);

  std::string_view header{header_buf, header_buf + hdr_size};
  BOOST_TEST(header == "\x09\x01\x02\x03\x04\x05\x06\x07\x08");
}
BOOST_AUTO_TEST_SUITE_END() // encode_header

BOOST_AUTO_TEST_SUITE(DecodeHeader)

BOOST_AUTO_TEST_CASE(FirstByteMismatchWithHeaderSizeIsError) {
  uint8_t header_buf[] = {0x01, 0x02}; // header size first byte is invalid
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  BOOST_TEST(ec == MSGSTREAM_HDR_SYNC);
}

BOOST_AUTO_TEST_CASE(ReturnsEmptyMessageSizeAsZero) {
  uint8_t header_buf[] = {0x02, 0x00};
  size_t n = 1;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  BOOST_TEST(!ec);
  BOOST_TEST(n == 0);
}

BOOST_AUTO_TEST_CASE(ReturnsSingleByteMessageSize) {
  uint8_t header_buf[] = {0x02, 0xa1};
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  BOOST_TEST(!ec);
  BOOST_TEST(n == 0xa1);
}

BOOST_AUTO_TEST_CASE(ReturnsFourByteMessageSizeFromLittleEndian) {
  uint8_t header_buf[] = {0x05, 0x01, 0x02, 0x03, 0x04};
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  BOOST_TEST(!ec);
  BOOST_TEST(n == 0x04030201);
}

BOOST_AUTO_TEST_CASE(ReturnsEightByteMessageSizeFromLittleEndian) {
  uint8_t header_buf[] = {0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  BOOST_TEST(!ec);
  BOOST_TEST(n == 0x0807060504030201);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(IncrementalRead)

BOOST_FIXTURE_TEST_CASE(DecodesOneByteAtATime, f) {
  constexpr size_t bufsz = 512;
  uint8_t recv[bufsz], send[bufsz];

  size_t msgsz = 300;

  for (int i = 0; i < msgsz; ++i) {
    send[i] = (2 * i) % 0x100;
  }

  uint8_t hdr_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size;
  int ec = msgstream_header_size(msgsz, &hdr_size);
  BOOST_ASSERT(ec == MSGSTREAM_OK);

  ec = msgstream_encode_header(msgsz, hdr_size, hdr_buf);
  BOOST_ASSERT(ec == MSGSTREAM_OK);

  auto reader = msgstream_incremental_reader_alloc(recv, msgsz);
  BOOST_ASSERT(reader);

  int is_complete;
  size_t recv_msg_size;

  for (int i = 0; i < hdr_size; ++i) {
    write(write_, &hdr_buf[i], 1);

    ec = msgstream_fd_incremental_recv(read_, reader, &is_complete,
                                       &recv_msg_size);
    BOOST_ASSERT(ec == MSGSTREAM_OK);
    BOOST_ASSERT(!is_complete);
  }

  for (int i = 0; i < msgsz - 1; ++i) {
    write(write_, &send[i], 1);

    ec = msgstream_fd_incremental_recv(read_, reader, &is_complete,
                                       &recv_msg_size);
    BOOST_ASSERT(ec == MSGSTREAM_OK);
    BOOST_ASSERT(!is_complete);
  }

  write(write_, &send[msgsz - 1], 1);
  ec = msgstream_fd_incremental_recv(read_, reader, &is_complete,
                                     &recv_msg_size);

  BOOST_TEST(ec == MSGSTREAM_OK);
  BOOST_TEST(is_complete);
  BOOST_TEST(recv_msg_size == msgsz);

  for (int i = 0; i < msgsz; ++i) {
    BOOST_TEST(recv[i] == ((2 * i) % 0x100));
  }

  msgstream_incremental_reader_free(reader);
}

BOOST_FIXTURE_TEST_CASE(CanReadTwoMessagesInARow, f) {
  std::string msg = "hello";
  std::array<char, 32> buf;

  auto reader = msgstream_incremental_reader_alloc(buf.data(), buf.size());
  BOOST_ASSERT(reader);

  msgstream_fd_send(write_, msg.data(), buf.size(), msg.size() + 1);

  int is_complete;
  size_t msgsz;
  do {
    int ec = msgstream_fd_incremental_recv(read_, reader, &is_complete, &msgsz);
    BOOST_ASSERT(ec == MSGSTREAM_OK);
  } while (!is_complete);

  // interpret as C string
  std::string_view hello{buf.data()};
  BOOST_TEST(hello == "hello");

  msg = "goodbye";

  msgstream_fd_send(write_, msg.data(), buf.size(), msg.size() + 1);

  do {
    int ec = msgstream_fd_incremental_recv(read_, reader, &is_complete, &msgsz);
    BOOST_ASSERT(ec == MSGSTREAM_OK);
  } while (!is_complete);

  std::string_view goodbye{buf.data()};
  BOOST_TEST(goodbye == "goodbye");

  msgstream_incremental_reader_free(reader);
}

BOOST_FIXTURE_TEST_CASE(MisMatchBufSizeIsError, f) {
  uint8_t b1 = 1;
  std::array<uint8_t, 0x10000> buf;
  auto reader = msgstream_incremental_reader_alloc(buf.data(), buf.size());

  // This will try to read 4 byte header. The sent message
  // is 3 bytes in total. Verify we don't just blindly read
  // the 3 bytes assuming the header is ok.

  int ec = msgstream_fd_send(write_, &b1, 1, 1);
  BOOST_ASSERT(ec == MSGSTREAM_OK);

  int is_complete;
  size_t msg_size;
  ec = msgstream_fd_incremental_recv(read_, reader, &is_complete, &msg_size);

  BOOST_TEST(ec == MSGSTREAM_HDR_SYNC);
}

BOOST_FIXTURE_TEST_CASE(NoDataOnNonBlockingFdIsNotError, f) {
  std::array<uint8_t, 512> buf;
  auto reader = msgstream_incremental_reader_alloc(buf.data(), buf.size());

  BOOST_ASSERT(fcntl(read_, F_SETFL, O_NONBLOCK) != -1);

  int is_complete;
  size_t msg_size;
  int ec =
      msgstream_fd_incremental_recv(read_, reader, &is_complete, &msg_size);

  BOOST_TEST(ec == MSGSTREAM_OK);
  BOOST_TEST(!is_complete);
}

BOOST_AUTO_TEST_SUITE_END()
