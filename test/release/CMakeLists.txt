cmake_minimum_required(VERSION 3.8)
project(MsgStreamTest)

find_package(msgstream REQUIRED)

add_executable(header_size
	header_size.c
)

target_link_libraries(header_size PRIVATE msgstream)
