/*
 * Android/Bionic compatibility functions used by the imported game library.
 */

#include "reimpl/bionic_compat.h"

#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <netdb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <psp2/kernel/processmgr.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#ifdef NDK_PORT
#include <falso_ndk/linux/fndk_unistd.h>
#endif

#include "reimpl/sys.h"
#include "utils/logger.h"
#include "utils/so_trace.h"

static int checked_size(const char *function, size_t requested, size_t available) {
    if (available == SIZE_MAX || requested <= available) return 1;
    l_fatal("FORTIFY: %s requested %lu bytes from a %lu-byte buffer",
            function, (unsigned long) requested, (unsigned long) available);
    errno = EOVERFLOW;
    return 0;
}

void *__memchr_chk_soloader(const void *src, int c, size_t count,
                            size_t src_size) {
    if (!checked_size("memchr", count, src_size)) return NULL;
    return memchr(src, c, count);
}

void *__memcpy_chk_soloader(void *dst, const void *src, size_t count,
                            size_t dst_size) {
    if (!checked_size("memcpy", count, dst_size)) return NULL;
    return memcpy(dst, src, count);
}

void *__memmove_chk_soloader(void *dst, const void *src, size_t count,
                             size_t dst_size) {
    if (!checked_size("memmove", count, dst_size)) return NULL;
    return memmove(dst, src, count);
}

void *__memset_chk_soloader(void *dst, int c, size_t count,
                            size_t dst_size) {
    if (!checked_size("memset", count, dst_size)) return NULL;
    return memset(dst, c, count);
}

size_t __strlen_chk_soloader(const char *src, size_t src_size) {
    size_t length = strnlen(src, src_size);
    if (length == src_size && src_size != SIZE_MAX) {
        checked_size("strlen", src_size + 1, src_size);
    }
    return length;
}

char *__strchr_chk_soloader(const char *src, int c, size_t src_size) {
    size_t length = __strlen_chk_soloader(src, src_size);
    if (length == src_size && src_size != SIZE_MAX) return NULL;
    return strchr(src, c);
}

char *__strcpy_chk_soloader(char *dst, const char *src, size_t dst_size) {
    size_t length = strlen(src) + 1;
    if (!checked_size("strcpy", length, dst_size)) return NULL;
    return strcpy(dst, src);
}

char *__strncpy_chk_soloader(char *dst, const char *src, size_t count,
                             size_t dst_size) {
    if (!checked_size("strncpy", count, dst_size)) return NULL;
    return strncpy(dst, src, count);
}

char *__strncpy_chk2_soloader(char *dst, const char *src, size_t count,
                              size_t dst_size, size_t src_size) {
    if (!checked_size("strncpy destination", count, dst_size)) return NULL;
    if (count > src_size && src_size != SIZE_MAX &&
        memchr(src, '\0', src_size) == NULL) {
        checked_size("strncpy source", count, src_size);
        return NULL;
    }
    return strncpy(dst, src, count);
}

size_t __fread_chk_soloader(void *buf, size_t size, size_t count,
                            FILE *stream, size_t buf_size) {
    size_t total;
    if (__builtin_mul_overflow(size, count, &total) ||
        !checked_size("fread", total, buf_size)) return 0;
#ifdef USE_SCELIBC_IO
    return sceLibcBridge_fread(buf, size, count, stream);
#else
    return fread(buf, size, count, stream);
#endif
}

size_t __fwrite_chk_soloader(const void *buf, size_t size, size_t count,
                             FILE *stream, size_t buf_size) {
    size_t total;
    if (__builtin_mul_overflow(size, count, &total) ||
        !checked_size("fwrite", total, buf_size)) return 0;
#ifdef USE_SCELIBC_IO
    return sceLibcBridge_fwrite(buf, size, count, stream);
#else
    return fwrite(buf, size, count, stream);
#endif
}

int __vsnprintf_chk_soloader(char *dst, size_t supplied_size, int flags,
                             size_t dst_size, const char *format, va_list args) {
    (void) flags;
    if (!checked_size("vsnprintf", supplied_size, dst_size)) return -1;
    return vsnprintf(dst, supplied_size, format, args);
}

int __vsprintf_chk_soloader(char *dst, int flags, size_t dst_size,
                            const char *format, va_list args) {
    (void) flags;
    size_t limit = dst_size == SIZE_MAX ? INT_MAX : dst_size;
    return vsnprintf(dst, limit, format, args);
}

ssize_t __read_chk_soloader(int fd, void *buf, size_t count,
                            size_t buf_size) {
    if (!checked_size("read", count, buf_size)) return -1;
#ifdef NDK_PORT
    return fndk_read(fd, buf, count);
#else
    return read(fd, buf, count);
#endif
}

ssize_t __write_chk_soloader(int fd, const void *buf, size_t count,
                             size_t buf_size) {
    if (!checked_size("write", count, buf_size)) return -1;
#ifdef NDK_PORT
    return fndk_write(fd, buf, count);
#else
    return write(fd, buf, count);
#endif
}

int __poll_chk_soloader(struct pollfd *fds, nfds_t count, int timeout,
                        size_t fds_size) {
    if (count > fds_size / sizeof(*fds)) {
        checked_size("poll", count * sizeof(*fds), fds_size);
        return -1;
    }
    return poll(fds, count, timeout);
}

void __FD_SET_chk_soloader(int fd, fd_set *set, size_t set_size) {
    if (fd < 0 || (size_t) fd / CHAR_BIT >= set_size) {
        l_fatal("FORTIFY: FD_SET received invalid descriptor %d", fd);
        errno = EINVAL;
        return;
    }
    FD_SET(fd, set);
}

bionic_cmsghdr *__cmsg_nxthdr_soloader(struct msghdr *msg,
                                       bionic_cmsghdr *cmsg) {
    if (!msg || !cmsg || cmsg->cmsg_len < sizeof(*cmsg)) return NULL;
    unsigned char *end = (unsigned char *) msg->msg_control + msg->msg_controllen;
    size_t aligned = (cmsg->cmsg_len + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);
    unsigned char *next = (unsigned char *) cmsg + aligned;
    if (next + sizeof(*cmsg) > end) return NULL;
    return (bionic_cmsghdr *) next;
}

size_t __ctype_get_mb_cur_max_soloader(void) {
    return MB_CUR_MAX;
}

int __register_atfork_soloader(void (*prepare)(void), void (*parent)(void),
                               void (*child)(void), void *dso_handle) {
    (void) prepare;
    (void) parent;
    (void) child;
    (void) dso_handle;
    return 0;
}

int accept4_soloader(int socket, struct sockaddr *address,
                     socklen_t *address_len, int flags) {
    (void) flags;
    return accept(socket, address, address_len);
}

int fseeko64_soloader(FILE *stream, long long offset, int whence) {
#ifdef USE_SCELIBC_IO
    if (offset > LONG_MAX || offset < LONG_MIN) {
        errno = EOVERFLOW;
        return -1;
    }
    return sceLibcBridge_fseek(stream, (long) offset, whence);
#else
    return fseeko(stream, (off_t) offset, whence);
#endif
}

unsigned long getauxval_soloader(unsigned long type) {
    /* Linux AT_PAGESZ. Other keys are intentionally reported unavailable. */
    if (type == 6) return PAGE_SIZE;
    errno = ENOENT;
    return 0;
}

int getifaddrs_soloader(void **ifap) {
    if (ifap) *ifap = NULL;
    errno = ENOSYS;
    return -1;
}

void freeifaddrs_soloader(void *ifa) {
    (void) ifa;
}

int getnameinfo_soloader(const struct sockaddr *address, socklen_t address_len,
                         char *host, socklen_t host_len, char *service,
                         socklen_t service_len, int flags) {
    (void) address;
    (void) address_len;
    (void) host;
    (void) host_len;
    (void) service;
    (void) service_len;
    (void) flags;
    return EAI_FAIL;
}

int getpwuid_r_soloader(unsigned int uid, void *pwd, char *buf,
                        size_t buf_len, void **result) {
    (void) uid;
    (void) pwd;
    (void) buf;
    (void) buf_len;
    if (result) *result = NULL;
    return ENOENT;
}

unsigned int if_nametoindex_soloader(const char *name) {
    return name && strcmp(name, "lo") == 0 ? 1 : 0;
}

int madvise_soloader(void *address, size_t length, int advice) {
    (void) address;
    (void) length;
    (void) advice;
    return 0;
}

int mlock_soloader(const void *address, size_t length) {
    (void) address;
    (void) length;
    return 0;
}

int mprotect_soloader(void *address, size_t length, int protection) {
    (void) address;
    (void) length;
    (void) protection;
    return 0;
}

char *mktemp_soloader(char *template_name) {
    static unsigned int sequence;
    static const char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (!template_name) {
        errno = EINVAL;
        return NULL;
    }

    size_t length = strlen(template_name);
    if (length < 6 || memcmp(template_name + length - 6, "XXXXXX", 6) != 0) {
        template_name[0] = '\0';
        errno = EINVAL;
        return template_name;
    }

    uint64_t value = (uint64_t) sceKernelGetProcessTimeWide();
    value ^= (uintptr_t) template_name;
    value ^= (uint64_t) __sync_fetch_and_add(&sequence, 1) << 24;
    for (unsigned int i = 0; i < 6; ++i) {
        template_name[length - 1 - i] = alphabet[value % 36];
        value /= 36;
    }
    return template_name;
}

int posix_memalign_soloader(void **result, size_t alignment, size_t size) {
    if (!result || alignment < sizeof(void *) ||
        (alignment & (alignment - 1)) != 0 || alignment % sizeof(void *) != 0) {
        return EINVAL;
    }
    void *allocation = memalign(alignment, size);
    if (!allocation) return ENOMEM;
    *result = allocation;
    return 0;
}

int recvmmsg_soloader(int socket, bionic_mmsghdr *messages,
                      unsigned int count, int flags, struct timespec *timeout) {
    (void) timeout;
    unsigned int completed = 0;
    for (; completed < count; ++completed) {
        ssize_t result = recvmsg(socket, &messages[completed].msg_hdr, flags);
        if (result < 0) return completed ? (int) completed : -1;
        messages[completed].msg_len = (unsigned int) result;
    }
    return (int) completed;
}

int sendmmsg_soloader(int socket, bionic_mmsghdr *messages,
                      unsigned int count, int flags) {
    unsigned int completed = 0;
    for (; completed < count; ++completed) {
        ssize_t result = sendmsg(socket, &messages[completed].msg_hdr, flags);
        if (result < 0) return completed ? (int) completed : -1;
        messages[completed].msg_len = (unsigned int) result;
    }
    return (int) completed;
}

_Static_assert(sizeof(bionic_sigaltstack) == 12,
               "Android ARM sigaltstack ABI must be 12 bytes");

typedef struct bionic_sigaltstack_state {
    bionic_sigaltstack current;
} bionic_sigaltstack_state;

static pthread_key_t bionic_sigaltstack_key;
static pthread_once_t bionic_sigaltstack_key_once = PTHREAD_ONCE_INIT;

static void destroy_bionic_sigaltstack_state(void *opaque) {
    free(opaque);
}

static void create_bionic_sigaltstack_key(void) {
    pthread_key_create(&bionic_sigaltstack_key,
                       destroy_bionic_sigaltstack_state);
}

static bionic_sigaltstack_state *get_bionic_sigaltstack_state(void) {
    pthread_once(&bionic_sigaltstack_key_once,
                 create_bionic_sigaltstack_key);
    bionic_sigaltstack_state *state =
        (bionic_sigaltstack_state *)pthread_getspecific(bionic_sigaltstack_key);
    if (!state) {
        state = (bionic_sigaltstack_state *)calloc(1, sizeof(*state));
        if (!state)
            return NULL;
        state->current.flags = 2; /* Android SS_DISABLE */
        pthread_setspecific(bionic_sigaltstack_key, state);
    }
    return state;
}

int sigaltstack_soloader(const bionic_sigaltstack *stack,
                         bionic_sigaltstack *old_stack) {
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    uintptr_t offset = 0;
    bionic_sigaltstack_state *state = get_bionic_sigaltstack_state();
    if (!state) {
        errno = ENOMEM;
        return -1;
    }

    if (so_trace_offset(caller, &offset))
        l_info("[signal] sigaltstack(new=%p old=%p) caller=so+0x%08X",
               (const void *)stack, (void *)old_stack, (unsigned)offset);
    else
        l_info("[signal] sigaltstack(new=%p old=%p) caller=%p",
               (const void *)stack, (void *)old_stack, (void *)caller);

    if (old_stack)
        *old_stack = state->current;

    if (stack) {
        if (stack->flags != 0 && stack->flags != 2) {
            errno = EINVAL;
            l_error("[signal] unsupported sigaltstack flags=0x%X",
                    (unsigned)stack->flags);
            return -1;
        }
        state->current = *stack;
        l_info("[signal] altstack sp=%p size=%u flags=0x%X",
               stack->sp, (unsigned)stack->size, (unsigned)stack->flags);
    }

    return 0;
}

int sigemptyset_soloader(uint32_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = 0;
    return 0;
}

void android_set_abort_message_soloader(const char *message) {
    l_error("Android abort message: %s", message ? message : "(null)");
}

void openlog_soloader(const char *ident, int option, int facility) {
    (void) ident;
    (void) option;
    (void) facility;
}

void closelog_soloader(void) {
}

void syslog_soloader(int priority, const char *format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    l_info("syslog[%d]: %s", priority, message);
}

int64_t AMotionEvent_getEventTime_soloader(const void *motion_event) {
    (void) motion_event;
    /* Android reports nanoseconds; Vita's process timer reports microseconds. */
    return (int64_t) sceKernelGetProcessTimeWide() * 1000;
}
