/* Android GLES exposes numeric buffer names. This VitaGL build exposes its
 * internal VBO pointer as GLuint, while the game's drawBatch_nogamma_VBO
 * stores the element-buffer name in an unsigned short. Return compact names
 * to the SO and translate them at every buffer-name API boundary. */
#define GUEST_BUFFER_COUNT 65536u
static GLuint guest_buffers[GUEST_BUFFER_COUNT];
static unsigned next_guest_buffer = 1;
static volatile int guest_buffers_lock;
static GLuint current_array_guest, current_array_vita;
static GLuint current_element_guest, current_element_vita;
#define LOAD32(p) __atomic_load_n((p), __ATOMIC_RELAXED)
#define STORE32(p,v) __atomic_store_n((p), (v), __ATOMIC_RELAXED)

static void lock_guest_buffers(void) {
    while (__sync_lock_test_and_set(&guest_buffers_lock, 1)) {
        while (guest_buffers_lock) { }
    }
}

static void unlock_guest_buffers(void) {
    __sync_lock_release(&guest_buffers_lock);
}

static GLuint reserve_guest_buffer(GLuint vita_name) {
    GLuint guest = 0;
    lock_guest_buffers();
    for (unsigned i = 0; i < GUEST_BUFFER_COUNT - 1; ++i) {
        unsigned candidate = next_guest_buffer;
        next_guest_buffer = candidate + 1;
        if (next_guest_buffer == GUEST_BUFFER_COUNT)
            next_guest_buffer = 1;
        if (!LOAD32(&guest_buffers[candidate])) {
            STORE32(&guest_buffers[candidate], vita_name);
            guest = candidate;
            break;
        }
    }
    unlock_guest_buffers();
    return guest;
}

void glGenBuffers_soloader(GLsizei n, GLuint *buffers) {
    #ifdef DEBUG_SOLOADER
    static unsigned calls;
    #endif
    if (n <= 0 || !buffers) {
        glGenBuffers(n, buffers);
        return;
    }
    for (GLsizei i = 0; i < n; ++i) {
        GLuint vita_name = 0;
        glGenBuffers(1, &vita_name);
        GLuint guest = vita_name ? reserve_guest_buffer(vita_name) : 0;
        if (vita_name && !guest) {
            l_error("[GL] guest buffer names exhausted");
            glDeleteBuffers(1, &vita_name);
        }
        buffers[i] = guest;
        #ifdef DEBUG_SOLOADER
        if (calls < 48 && calls++ < 48)
            l_info("[GL] glGenBuffers caller=%p guest=%u vita=0x%08X",
                   __builtin_return_address(0), (unsigned)guest,
                   (unsigned)vita_name);
        #endif
    }
}

void glBindBuffer_soloader(GLenum target, GLuint buffer) {
    gl_perf.binds++;
#ifdef DEBUG_SOLOADER
    static unsigned element_calls;
#endif
    GLuint vita_name = 0;
    if (buffer) {
        if (buffer >= GUEST_BUFFER_COUNT) {
            l_error("[GL] glBindBuffer guest name exceeds 16 bits: 0x%08X caller=%p",
                        (unsigned)buffer, __builtin_return_address(0));
            return;
        }
        vita_name = LOAD32(&guest_buffers[buffer]);
        if (!vita_name) {
            /* GLES permits binding a previously unused nonzero name. */
            glGenBuffers(1, &vita_name);
            if (!vita_name)
                return;
            lock_guest_buffers();
            if (LOAD32(&guest_buffers[buffer])) {
                GLuint existing = LOAD32(&guest_buffers[buffer]);
                unlock_guest_buffers();
                glDeleteBuffers(1, &vita_name);
                vita_name = existing;
            } else {
                STORE32(&guest_buffers[buffer], vita_name);
                unlock_guest_buffers();
            }
            l_info("[GL] created implicit buffer guest=%u vita=0x%08X",
                       (unsigned)buffer, (unsigned)vita_name);
        }
    }
    #ifdef DEBUG_SOLOADER
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        if (element_calls < 64 && element_calls++ < 64)
            l_info("[GL] glBindBuffer(ELEMENT) caller=%p guest=%u vita=0x%08X",
                   __builtin_return_address(0), (unsigned)buffer,
                   (unsigned)vita_name);
    }
    #endif
    glBindBuffer(target, vita_name);
    if (target == GL_ARRAY_BUFFER) {
        STORE32(&current_array_vita, vita_name);
        STORE32(&current_array_guest, buffer);
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        STORE32(&current_element_vita, vita_name);
        STORE32(&current_element_guest, buffer);
    }
}

void glDeleteBuffers_soloader(GLsizei n, const GLuint *buffers) {
    if (n <= 0 || !buffers) {
        glDeleteBuffers(n, buffers);
        return;
    }
    for (GLsizei i = 0; i < n; ++i) {
        GLuint guest = buffers[i];
        GLuint vita_name = 0;
        if (guest && guest < GUEST_BUFFER_COUNT) {
            lock_guest_buffers();
            vita_name = LOAD32(&guest_buffers[guest]);
            STORE32(&guest_buffers[guest], 0);
            if (LOAD32(&current_array_guest) == guest) {
                /* This pinned VitaGL frees VBOs without unbinding them. */
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                STORE32(&current_array_guest, 0); STORE32(&current_array_vita, 0);
            }
            if (LOAD32(&current_element_guest) == guest) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                STORE32(&current_element_guest, 0); STORE32(&current_element_vita, 0);
            }
            unlock_guest_buffers();
        }
        if (vita_name)
            glDeleteBuffers(1, &vita_name);
    }
}

void glGetIntegerv_soloader(GLenum pname, GLint *data) {
    if (pname == GL_ARRAY_BUFFER_BINDING || pname == GL_ELEMENT_ARRAY_BUFFER_BINDING) {
        if (data) *data = (GLint)LOAD32(pname == GL_ARRAY_BUFFER_BINDING ?
                                  &current_array_guest : &current_element_guest);
        return;
    }
    glGetIntegerv(pname, data);
}

void glDrawElements_soloader(GLenum mode, GLsizei count, GLenum type,
                            const GLvoid *indices) {
    gl_perf.draws++;
    GLuint element_binding = LOAD32(&current_element_vita);
#ifdef DEBUG_SOLOADER
    static unsigned draws;
    if (draws < 16 && draws++ < 16)
        l_info("[GL] glDrawElements caller=%p mode=0x%X count=%d type=0x%X indices=%p element=0x%08X",
               __builtin_return_address(0), (unsigned)mode, (int)count,
               (unsigned)type, indices, (unsigned)element_binding);
#endif
    if (element_binding != 0 && (uint32_t)element_binding < 0x80000000u) {
        gl_perf.rejected++;
        /* Count every rejected draw; emit only the first four diagnostics. */
        if (gl_perf.rejected <= 4) l_error("[GL] skip draw: invalid VitaGL element-buffer pointer 0x%08X",
                    (unsigned)element_binding);
        return;
    }
    glDrawElements(mode, count, type, indices);
}

