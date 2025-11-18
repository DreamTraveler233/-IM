#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>

namespace CIM {
// 用于检查钩子功能是否启用
bool is_hook_enable();
// 用于设置钩子功能的启用状态
void set_hook_enable(bool flag);
}  // namespace CIM

// 这些函数指针变量不会被C++名称修饰
extern "C" {
// sleep
typedef unsigned int (*sleep_fun)(unsigned int seconds);
typedef int (*usleep_fun)(useconds_t usec);
typedef int (*nanosleep_fun)(const struct timespec* req, struct timespec* rem);
extern sleep_fun sleep_f;
extern usleep_fun usleep_f;
extern nanosleep_fun nanosleep_f;

// socket
typedef int (*socket_fun)(int domain, int type, int protocol);
typedef int (*connect_fun)(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
typedef int (*accept_fun)(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
extern socket_fun socket_f;
extern connect_fun connect_f;
extern accept_fun accept_f;

// read
typedef ssize_t (*read_fun)(int fd, void* buf, size_t count);
typedef ssize_t (*readv_fun)(int fd, const struct iovec* iov, int iovcnt);
typedef ssize_t (*recv_fun)(int sockfd, void* buf, size_t len, int flags);
typedef ssize_t (*recvfrom_fun)(int sockfd, void* buf, size_t len, int flags,
                                struct sockaddr* src_addr, socklen_t* addrlen);
typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr* msg, int flags);
extern read_fun read_f;
extern readv_fun readv_f;
extern recv_fun recv_f;
extern recvfrom_fun recvfrom_f;
extern recvmsg_fun recvmsg_f;

// write
typedef ssize_t (*write_fun)(int fd, const void* buf, size_t count);
typedef ssize_t (*writev_fun)(int fd, const struct iovec* iov, int iovcnt);
typedef ssize_t (*send_fun)(int sockfd, const void* buf, size_t len, int flags);
typedef ssize_t (*sendto_fun)(int sockfd, const void* buf, size_t len, int flags,
                              const struct sockaddr* dest_addr, socklen_t addrlen);
typedef ssize_t (*sendmsg_fun)(int sockfd, const struct msghdr* msg, int flags);
extern write_fun write_f;
extern writev_fun writev_f;
extern send_fun send_f;
extern sendto_fun sendto_f;
extern sendmsg_fun sendmsg_f;

// close
typedef int (*close_fun)(int fd);
extern close_fun close_f;

// fcntl
typedef int (*fcntl_fun)(int fd, int op, ...);
typedef int (*ioctl_fun)(int fd, unsigned long request, ...);
typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void* optval, socklen_t* optlen);
typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void* optval,
                              socklen_t optlen);
extern fcntl_fun fcntl_f;
extern ioctl_fun ioctl_f;
extern getsockopt_fun getsockopt_f;
extern setsockopt_fun setsockopt_f;

extern int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                                uint64_t timeout_ms);
}