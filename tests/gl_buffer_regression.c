/* Exercise the real guest bridge against a pointer-name VitaGL mock. */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef unsigned GLuint, GLenum; typedef int GLint, GLsizei; typedef void GLvoid;
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define l_info(...) ((void)0)
#define l_error(...) ((void)0)
static struct { unsigned binds,draws,rejected; } gl_perf;
static unsigned mock_next=0x81234500, array,element,draw_calls,query_calls;
void glGenBuffers(GLsizei n,GLuint *p) { for(int i=0;i<n;++i) p[i]=mock_next+=0x100; }
void glBindBuffer(GLenum t,GLuint p) { if(t==GL_ARRAY_BUFFER) array=p; if(t==GL_ELEMENT_ARRAY_BUFFER) element=p; }
void glDeleteBuffers(GLsizei n,const GLuint *p) { /* Match pinned VitaGL: deletion does not unbind. */ }
void glGetIntegerv(GLenum p,GLint *v) { query_calls++; if(v) *v=123; }
void glDrawElements(GLenum m,GLsizei n,GLenum t,const void *p) { draw_calls++; }
#include "utils/gl_buffers.inc"
int main(void) {
    GLuint b[2]; GLint q;
    glGenBuffers_soloader(2,b);
    assert(b[0] && b[0]<=65535 && b[1]!=b[0]);
    uint16_t ebo=(uint16_t)b[0];
    glBindBuffer_soloader(GL_ELEMENT_ARRAY_BUFFER,ebo);
    assert(element==guest_buffers[ebo] && element>0x80000000);
    glBindBuffer_soloader(GL_ARRAY_BUFFER,b[1]);
    glGetIntegerv_soloader(GL_ELEMENT_ARRAY_BUFFER_BINDING,&q); assert(q==ebo);
    glGetIntegerv_soloader(GL_ARRAY_BUFFER_BINDING,&q); assert(q==b[1]);
    for(int i=0;i<10000;++i) glDrawElements_soloader(4,4,0x1403,0);
    assert(draw_calls==10000 && query_calls==0);
    glGetIntegerv_soloader(0x123,&q); assert(q==123 && query_calls==1);
    glBindBuffer_soloader(GL_ARRAY_BUFFER,ebo);
    glDeleteBuffers_soloader(1,b);
    assert(!array && !element && !current_array_vita && !current_element_vita);
    glGetIntegerv_soloader(GL_ELEMENT_ARRAY_BUFFER_BINDING,&q); assert(!q);
    glGetIntegerv_soloader(GL_ARRAY_BUFFER_BINDING,&q); assert(!q);
    glBindBuffer_soloader(GL_ELEMENT_ARRAY_BUFFER,65535); assert(element>0x80000000);
    assert(guest_buffers[65535]==element);
    glBindBuffer_soloader(GL_ELEMENT_ARRAY_BUFFER,65536); assert(current_element_guest==65535);
    glBindBuffer_soloader(0xdead,b[1]); assert(current_element_guest==65535 && current_array_guest==0);
    glBindBuffer_soloader(GL_ELEMENT_ARRAY_BUFFER,0); assert(!element);
    glDrawElements_soloader(4,4,0x1403,(void*)0x1000); assert(draw_calls==10001);
    STORE32(&current_element_vita,0x776f);
    glDrawElements_soloader(4,4,0x1403,0); assert(draw_calls==10001 && gl_perf.rejected==1);
    memset(guest_buffers,0xff,sizeof(guest_buffers));
    assert(reserve_guest_buffer(0x81230000)==0);
    guest_buffers[42]=0; assert(reserve_guest_buffer(0x81230000)==42);
    puts("GL regression passed: guest IDs, full-width pointers, cached queries, deletion, invalid EBO guard, exhaustion");
}
