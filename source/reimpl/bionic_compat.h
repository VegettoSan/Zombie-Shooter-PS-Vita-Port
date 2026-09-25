/*
 * Compatibility helpers for Android/Bionic symbols that do not exist in
 * newlib or whose ABI needs a small translation before calling VitaSDK.
 */

#ifndef SOLOADER_BIONIC_COMPAT_H
#define SOLOADER_BIONIC_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>

typedef struct bionic_mmsghdr {
    struct msghdr msg_hdr;
    unsigned int msg_len;
} bionic_mmsghdr;

typedef struct bionic_cmsghdr {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
} bionic_cmsghdr;

void *__memchr_chk_soloader(const void *src, int c, size_t count,
                            size_t src_size);
void *__memcpy_chk_soloader(void *dst, const void *src, size_t count,
                            size_t dst_size);
void *__memmove_chk_soloader(void *dst, const void *src, size_t count,
                             size_t dst_size);
void *__memset_chk_soloader(void *dst, int c, size_t count,
                            size_t dst_size);
char *__strchr_chk_soloader(const char *src, int c, size_t src_size);
char *__strcpy_chk_soloader(char *dst, const char *src, size_t dst_size);
size_t __strlen_chk_soloader(const char *src, size_t src_size);
char *__strncpy_chk_soloader(char *dst, const char *src, size_t count,
                             size_t dst_size);
char *__strncpy_chk2_soloader(char *dst, const char *src, size_t count,
                              size_t dst_size, size_t src_size);

size_t __fread_chk_soloader(void *buf, size_t size, size_t count,
                            FILE *stream, size_t buf_size);
size_t __fwrite_chk_soloader(const void *buf, size_t size, size_t count,
                             FILE *stream, size_t buf_size);
int __vsnprintf_chk_soloader(char *dst, size_t supplied_size, int flags,
                             size_t dst_size, const char *format, va_list args);
int __vsprintf_chk_soloader(char *dst, int flags, size_t dst_size,
                            const char *format, va_list args);
ssize_t __read_chk_soloader(int fd, void *buf, size_t count,
                            size_t buf_size);
ssize_t __write_chk_soloader(int fd, const void *buf, size_t count,
                             size_t buf_size);
int __poll_chk_soloader(struct pollfd *fds, nfds_t count, int timeout,
                        size_t fds_size);
void __FD_SET_chk_soloader(int fd, fd_set *set, size_t set_size);
bionic_cmsghdr *__cmsg_nxthdr_soloader(struct msghdr *msg,
                                       bionic_cmsghdr *cmsg);
size_t __ctype_get_mb_cur_max_soloader(void);
int __register_atfork_soloader(void (*prepare)(void), void (*parent)(void),
                               void (*child)(void), void *dso_handle);

int accept4_soloader(int socket, struct sockaddr *address,
                     socklen_t *address_len, int flags);
int fseeko64_soloader(FILE *stream, long long offset, int whence);
unsigned long getauxval_soloader(unsigned long type);
int getifaddrs_soloader(void **ifap);
void freeifaddrs_soloader(void *ifa);
int getnameinfo_soloader(const struct sockaddr *address, socklen_t address_len,
                         char *host, socklen_t host_len, char *service,
                         socklen_t service_len, int flags);
int getpwuid_r_soloader(unsigned int uid, void *pwd, char *buf,
                        size_t buf_len, void **result);
unsigned int if_nametoindex_soloader(const char *name);
int madvise_soloader(void *address, size_t length, int advice);
int mlock_soloader(const void *address, size_t length);
int mprotect_soloader(void *address, size_t length, int protection);
char *mktemp_soloader(char *template_name);
int posix_memalign_soloader(void **result, size_t alignment, size_t size);
int recvmmsg_soloader(int socket, bionic_mmsghdr *messages,
                      unsigned int count, int flags, struct timespec *timeout);
int sendmmsg_soloader(int socket, bionic_mmsghdr *messages,
                      unsigned int count, int flags);
typedef struct bionic_sigaltstack {
    void *sp;
    int32_t flags;
    uint32_t size;
} bionic_sigaltstack;

int sigaltstack_soloader(const bionic_sigaltstack *stack,
                         bionic_sigaltstack *old_stack);
int sigemptyset_soloader(uint32_t *set);

void android_set_abort_message_soloader(const char *message);
void openlog_soloader(const char *ident, int option, int facility);
void closelog_soloader(void);
void syslog_soloader(int priority, const char *format, ...);

int64_t AMotionEvent_getEventTime_soloader(const void *motion_event);

#ifdef __cplusplus
}
#endif

#endif // SOLOADER_BIONIC_COMPAT_H
