/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#include <gtest/gtest.h>

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

TEST(MsgStream, OkIsFalsy) { EXPECT_FALSE(MSGSTREAM_OK); }

TEST(MsgStream, ErrNameOk) {
  std::string_view nm{msgstream_errname(MSGSTREAM_OK)};
  EXPECT_EQ(nm, "MSGSTREAM_OK");
}

TEST(MsgStream, ErrStrOk) {
  std::string_view str{msgstream_errstr(MSGSTREAM_OK)};
  EXPECT_EQ(str, "no error detected");
}

class f : public testing::Test {
protected:
  void SetUp() override {
    int fds[2];
    if (pipe(fds) == -1) {
      perror("pipe");
      ADD_FAILURE() << "Failed to allocate pipe";
    }

    read_ = fds[0];
    write_ = fds[1];
  }

  void TearDown() override {
    close(read_);
    close(write_);
  }

  int read_;
  int write_;
};

TEST_F(f, TransferSingleByte) {
  int8_t b42 = 42, recv = 0;
  auto ec = msgstream_fd_send(write_, &b42, sizeof(b42), 1);
  EXPECT_FALSE(ec);

  size_t size = 0;
  ec = msgstream_fd_recv(read_, &recv, sizeof(recv), &size);
  EXPECT_FALSE(ec);

  EXPECT_EQ(size, 1);
  EXPECT_EQ(recv, 42);
}

TEST_F(f, EofIsDistinctError) {
  int8_t recv = 0;
  close(write_);
  size_t size = 1;
  auto ec = msgstream_fd_recv(read_, &recv, sizeof(recv), &size);

  EXPECT_EQ(ec, MSGSTREAM_EOF);
  EXPECT_EQ(size, 0);
}

TEST_F(f, TransferEmptyMsg) {
  int8_t b42 = 42, recv = 96;
  auto ec = msgstream_fd_send(write_, &b42, sizeof(b42), 0);
  EXPECT_FALSE(ec);

  size_t size = 1;
  ec = msgstream_fd_recv(read_, &recv, sizeof(recv), &size);
  EXPECT_FALSE(ec);
  EXPECT_EQ(size, 0);

  EXPECT_EQ(recv, 96); // no change
}

TEST_F(f, TransferStringHello) {
  char hello[9] = "hello", recv[9] = {};
  size_t len = strlen(hello);

  auto ec = msgstream_fd_send(write_, hello, sizeof(hello), len);
  EXPECT_FALSE(ec);

  size_t size = 0;
  ec = msgstream_fd_recv(read_, recv, sizeof(recv), &size);
  EXPECT_FALSE(ec);
  EXPECT_EQ(size, len);

  std::string_view recv_sv{recv, recv + size};
  EXPECT_EQ(recv_sv, "hello");
}

#undef HUGE
#define HUGE 0x12345

TEST_F(f, TransferHugeMessage) {
  uint8_t huge[HUGE], recv[HUGE] = {};

  for (size_t i = 0; i < HUGE; ++i)
    huge[i] = i % 256;

  int sret = MSGSTREAM_EOF;
  std::thread th{[&] { sret = msgstream_fd_send(write_, huge, HUGE, HUGE); }};

  size_t size;
  auto rret = msgstream_fd_recv(read_, recv, HUGE, &size);
  th.join();

  EXPECT_EQ(rret, MSGSTREAM_OK);
  EXPECT_EQ(sret, MSGSTREAM_OK);
  EXPECT_EQ(size, HUGE);

  for (size_t i = 0; i < HUGE; ++i) {
    if (recv[i] != (i % 256)) {
      std::ostringstream os;
      os << "Failed to read byte at index " << i << ". Expected " << (i % 256)
         << " but received " << recv[i];

      FAIL() << os.str();
    }
  }
}

#define EXPAND(X) X

#define DO_TEST(BUF_SZ, RET)                                                   \
  {                                                                            \
    size_t b##BUF_SZ;                                                          \
    int ec = msgstream_header_size(BUF_SZ, &EXPAND(b##BUF_SZ));                \
    EXPECT_FALSE(ec);                                                          \
    EXPECT_EQ(EXPAND(b##BUF_SZ), RET);                                         \
  }

TEST(HeaderSize, EmptyBufferIsMeaninglessAndErrors) {
  size_t n = 1;
  int ec = msgstream_header_size(0, &n);
  EXPECT_EQ(ec, MSGSTREAM_SMALL_BUF);
}

TEST(HeaderSize, TwoByteHeader){DO_TEST(0x1, 2) DO_TEST(0xff, 2)}

TEST(HeaderSize, ThreeByteHeader){DO_TEST(0x100, 3) DO_TEST(0xffff, 3)}

TEST(HeaderSize, FourByteHeader){DO_TEST(0x10000, 4) DO_TEST(0xffffff, 4)}

TEST(HeaderSize, FiveByteHeader){DO_TEST(0x1000000, 5) DO_TEST(0xffffffff, 5)}

TEST(HeaderSize,
     SixByteHeader){DO_TEST(0x100000000, 6) DO_TEST(0xffffffffff, 6)}

TEST(HeaderSize,
     SevenByteHeader){DO_TEST(0x10000000000, 7) DO_TEST(0xffffffffffff, 7)}

TEST(HeaderSize,
     EightByteHeader){DO_TEST(0x1000000000000, 8) DO_TEST(0xffffffffffffff, 8)}

TEST(HeaderSize, NineByteHeader){DO_TEST(0x100000000000000, 9)
                                     DO_TEST(0xffffffffffffffff, 9)}

#undef EXPAND
#undef DO_TEST

TEST(EncodeHeader, EmptyBufferIsMeaninglessAndErrors) {
  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ec = msgstream_encode_header(0, 0, header_buf);
  EXPECT_EQ(ec, MSGSTREAM_SMALL_HDR);
}

TEST(EncodeHeader, MsgSizeLargerThanBufSizeIsError) {
  uint8_t header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  auto ec = msgstream_encode_header(0xffff, 2, header_buf);
  EXPECT_EQ(ec, MSGSTREAM_BIG_MSG);
}

TEST(EncodeHeader, EncodesSingleByteMessageHeaderAsTwoBytes) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size = 2;
  auto ec = msgstream_encode_header(1, hdr_size, header_buf);
  EXPECT_FALSE(ec);

  std::string_view header{header_buf, header_buf + hdr_size};
  EXPECT_EQ(header, "\x02\x01");
}

TEST(EncodeHeader, EncodesMultiByteHeaderWithLittleEndianMsgSize) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size = 5;
  auto ec = msgstream_encode_header(0x04030201, hdr_size, header_buf);
  EXPECT_FALSE(ec);

  std::string_view header{header_buf, header_buf + hdr_size};
  EXPECT_EQ(header, "\x05\x01\x02\x03\x04");
}

TEST(EncodeHeader, EncodesNineByteHeaderWithLittleEndianMsgSize) {
  char header_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size = 9;
  auto ec = msgstream_encode_header(0x0807060504030201, hdr_size, header_buf);
  EXPECT_FALSE(ec);

  std::string_view header{header_buf, header_buf + hdr_size};
  EXPECT_EQ(header, "\x09\x01\x02\x03\x04\x05\x06\x07\x08");
}

TEST(DecodeHeader, FirstByteMismatchWithHeaderSizeIsError) {
  uint8_t header_buf[] = {0x01, 0x02}; // header size first byte is invalid
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  EXPECT_EQ(ec, MSGSTREAM_HDR_SYNC);
}

TEST(DecodeHeader, ReturnsEmptyMessageSizeAsZero) {
  uint8_t header_buf[] = {0x02, 0x00};
  size_t n = 1;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  EXPECT_FALSE(ec);
  EXPECT_EQ(n, 0);
}

TEST(DecodeHeader, ReturnsSingleByteMessageSize) {
  uint8_t header_buf[] = {0x02, 0xa1};
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  EXPECT_FALSE(ec);
  EXPECT_EQ(n, 0xa1);
}

TEST(DecodeHeader, ReturnsFourByteMessageSizeFromLittleEndian) {
  uint8_t header_buf[] = {0x05, 0x01, 0x02, 0x03, 0x04};
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  EXPECT_FALSE(ec);
  EXPECT_EQ(n, 0x04030201);
}

TEST(DecodeHeader, ReturnsEightByteMessageSizeFromLittleEndian) {
  uint8_t header_buf[] = {0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  size_t n = 0;
  auto ec = msgstream_decode_header(header_buf, sizeof(header_buf), &n);
  EXPECT_FALSE(ec);
  EXPECT_EQ(n, 0x0807060504030201);
}

TEST_F(f, DecodesOneByteAtATime) {
  constexpr size_t bufsz = 512;
  uint8_t recv[bufsz], send[bufsz];

  size_t msgsz = 300;

  for (int i = 0; i < msgsz; ++i) {
    send[i] = (2 * i) % 0x100;
  }

  uint8_t hdr_buf[MSGSTREAM_HEADER_BUF_SIZE];
  size_t hdr_size;
  int ec = msgstream_header_size(msgsz, &hdr_size);
  ASSERT_EQ(ec, MSGSTREAM_OK);

  ec = msgstream_encode_header(msgsz, hdr_size, hdr_buf);
  ASSERT_EQ(ec, MSGSTREAM_OK);

  auto reader = msgstream_incremental_reader_alloc(recv, msgsz);
  ASSERT_TRUE(reader);

  int is_complete;
  size_t recv_msg_size;

  for (int i = 0; i < hdr_size; ++i) {
    write(write_, &hdr_buf[i], 1);

    ec = msgstream_fd_incremental_recv(read_, reader, &is_complete,
                                       &recv_msg_size);
    ASSERT_EQ(ec, MSGSTREAM_OK);
    ASSERT_FALSE(is_complete);
  }

  for (int i = 0; i < msgsz - 1; ++i) {
    write(write_, &send[i], 1);

    ec = msgstream_fd_incremental_recv(read_, reader, &is_complete,
                                       &recv_msg_size);
    ASSERT_EQ(ec, MSGSTREAM_OK);
    ASSERT_FALSE(is_complete);
  }

  write(write_, &send[msgsz - 1], 1);
  ec = msgstream_fd_incremental_recv(read_, reader, &is_complete,
                                     &recv_msg_size);

  EXPECT_EQ(ec, MSGSTREAM_OK);
  EXPECT_TRUE(is_complete);
  EXPECT_EQ(recv_msg_size, msgsz);

  for (int i = 0; i < msgsz; ++i) {
    EXPECT_EQ(recv[i], ((2 * i) % 0x100));
  }

  msgstream_incremental_reader_free(reader);
}

TEST_F(f, CanReadTwoMessagesInARow) {
  std::string msg = "hello";
  std::array<char, 32> buf;

  auto reader = msgstream_incremental_reader_alloc(buf.data(), buf.size());
  ASSERT_TRUE(reader);

  msgstream_fd_send(write_, msg.data(), buf.size(), msg.size() + 1);

  int is_complete;
  size_t msgsz;
  do {
    int ec = msgstream_fd_incremental_recv(read_, reader, &is_complete, &msgsz);
    ASSERT_EQ(ec, MSGSTREAM_OK);
  } while (!is_complete);

  // interpret as C string
  std::string_view hello{buf.data()};
  EXPECT_EQ(hello, "hello");

  msg = "goodbye";

  msgstream_fd_send(write_, msg.data(), buf.size(), msg.size() + 1);

  do {
    int ec = msgstream_fd_incremental_recv(read_, reader, &is_complete, &msgsz);
    ASSERT_EQ(ec, MSGSTREAM_OK);
  } while (!is_complete);

  std::string_view goodbye{buf.data()};
  EXPECT_EQ(goodbye, "goodbye");

  msgstream_incremental_reader_free(reader);
}

TEST_F(f, MisMatchBufSizeIsError) {
  uint8_t b1 = 1;
  std::array<uint8_t, 0x10000> buf;
  auto reader = msgstream_incremental_reader_alloc(buf.data(), buf.size());

  // This will try to read 4 byte header. The sent message
  // is 3 bytes in total. Verify we don't just blindly read
  // the 3 bytes assuming the header is ok.

  int ec = msgstream_fd_send(write_, &b1, 1, 1);
  ASSERT_EQ(ec, MSGSTREAM_OK);

  int is_complete;
  size_t msg_size;
  ec = msgstream_fd_incremental_recv(read_, reader, &is_complete, &msg_size);

  EXPECT_EQ(ec, MSGSTREAM_HDR_SYNC);
}

TEST_F(f, NoDataOnNonBlockingFdIsNotError) {
  std::array<uint8_t, 512> buf;
  auto reader = msgstream_incremental_reader_alloc(buf.data(), buf.size());

  ASSERT_FALSE(fcntl(read_, F_SETFL, O_NONBLOCK) == -1);

  int is_complete;
  size_t msg_size;
  int ec =
      msgstream_fd_incremental_recv(read_, reader, &is_complete, &msg_size);

  EXPECT_EQ(ec, MSGSTREAM_OK);
  EXPECT_FALSE(is_complete);
}
