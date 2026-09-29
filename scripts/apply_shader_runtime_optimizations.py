#!/usr/bin/env python3
"""Apply conservative shader/runtime optimizations to the first-party GL bridge.

The actual game GLSL still goes through VitaGL/ShaccCg.  This layer only:
- canonicalizes the two observed whitespace-only duplicate texture shaders;
- dumps unique GLSL once in Debug for the MetalSyntax offline pipeline;
- skips redundant glUseProgram calls for programs known to have linked;
- caches uniform/attribute locations and invalidates on relink/delete.

All replacements are exact-anchor guarded and idempotent.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GLUTIL = ROOT / "source/utils/glutil.c"
DYNLIB = ROOT / "source/dynlib.c"
MARKER = "ZOMBIE_SHADER_RUNTIME_OPT_V1"

runtime = r'''
/* ZOMBIE_SHADER_RUNTIME_OPT_V1
 * MetalSyntax-inspired low-risk shader work: eliminate repeated CPU-side GL
 * work and make real game GLSL available to the offline semantic pipeline.
 * No hand-translated shader is trusted at runtime here. */
#define SHADER_LOOKUP_CACHE 96u
#define SHADER_KNOWN_PROGRAMS 64u
#define SHADER_NAME_MAX 64u

typedef struct {
    GLuint program;
    GLint location;
    unsigned stamp;
    char name[SHADER_NAME_MAX];
} ShaderLookupEntry;
static ShaderLookupEntry shader_uniform_cache[SHADER_LOOKUP_CACHE];
static ShaderLookupEntry shader_attrib_cache[SHADER_LOOKUP_CACHE];
static GLuint shader_known_programs[SHADER_KNOWN_PROGRAMS];
static unsigned shader_known_program_count, shader_lookup_stamp;
static GLuint shader_current_program = ~0u;
static unsigned shader_use_calls, shader_use_skips;
static unsigned shader_uniform_calls, shader_uniform_hits;
static unsigned shader_attrib_calls, shader_attrib_hits;
static unsigned shader_sources_deduped;
#ifdef ZOMBIE_DEBUG_BUILD
static unsigned shader_sources_dumped;
#endif

static int shader_program_known(GLuint program) {
    if (!program) return 1;
    for (unsigned i=0;i<shader_known_program_count;i++)
        if (shader_known_programs[i] == program) return 1;
    return 0;
}
static void shader_program_set_known(GLuint program, int known) {
    unsigned i=0;
    while (i<shader_known_program_count && shader_known_programs[i]!=program) i++;
    if (known) {
        if (i==shader_known_program_count && i<SHADER_KNOWN_PROGRAMS)
            shader_known_programs[shader_known_program_count++]=program;
    } else if (i<shader_known_program_count) {
        shader_known_programs[i]=shader_known_programs[--shader_known_program_count];
    }
}
static void shader_lookup_invalidate(GLuint program) {
    for (unsigned i=0;i<SHADER_LOOKUP_CACHE;i++) {
        if (shader_uniform_cache[i].program==program) shader_uniform_cache[i].program=0;
        if (shader_attrib_cache[i].program==program) shader_attrib_cache[i].program=0;
    }
    if (shader_current_program==program) shader_current_program=~0u;
}
static GLint shader_lookup_get(ShaderLookupEntry *cache, GLuint program,
                               const char *name, int attrib) {
    if (!name) return attrib ? glGetAttribLocation(program,name) : glGetUniformLocation(program,name);
    unsigned len=(unsigned)strlen(name);
    if (!len || len>=SHADER_NAME_MAX) return attrib ? glGetAttribLocation(program,name) : glGetUniformLocation(program,name);
    ShaderLookupEntry *slot=NULL;
    unsigned oldest=~0u;
    for (unsigned i=0;i<SHADER_LOOKUP_CACHE;i++) {
        ShaderLookupEntry *e=&cache[i];
        if (e->program==program && !strcmp(e->name,name)) {
            e->stamp=++shader_lookup_stamp;
            if (attrib) shader_attrib_hits++; else shader_uniform_hits++;
            return e->location;
        }
        if (!e->program) { slot=e; oldest=0; }
        else if (!slot || e->stamp<oldest) { slot=e; oldest=e->stamp; }
    }
    GLint loc=attrib ? glGetAttribLocation(program,name) : glGetUniformLocation(program,name);
    if (slot) {
        slot->program=program;slot->location=loc;slot->stamp=++shader_lookup_stamp;
        memcpy(slot->name,name,len+1);
    }
    return loc;
}

/* The 3.6.1 binary contains two actually-used fragment shaders whose only
 * difference is formatting whitespace.  Canonicalizing exactly this proven
 * shape lets VitaGL compile/cache one program instead of two without changing
 * precision, uniforms, varyings, or expression semantics. */
static const char zombie_plain_texture_fs[] =
    "precision mediump float;\n"
    "precision lowp int;\n"
    "varying vec2 v_texCoord;\n"
    "uniform lowp sampler2D s_texture;\n"
    "void main(){gl_FragColor=texture2D(s_texture,v_texCoord);}\n";
static int shader_plain_texture_duplicate(const char *s,size_t n) {
    if (!s || (n!=282u && n!=425u)) return 0;
    return strstr(s,"precision mediump float;") &&
           strstr(s,"uniform lowp sampler2D s_texture;") &&
           strstr(s,"varying vec2 v_texCoord;") &&
           strstr(s,"gl_FragColor") && strstr(s,"texture2D") &&
           !strstr(s,"discard") && !strstr(s,"u_gamma") &&
           !strstr(s,"u_diffuse") && !strstr(s,"u_specular") &&
           !strstr(s,"v_color");
}
static char *shader_concat_source(GLsizei count,const GLchar **strings,const GLint *lengths,size_t *out_n) {
    if (out_n) *out_n=0;
    if (count<=0 || !strings) return NULL;
    size_t total=0;
    for (GLsizei i=0;i<count;i++) {
        if (!strings[i]) return NULL;
        size_t n=(lengths && lengths[i]>=0)?(size_t)lengths[i]:strlen(strings[i]);
        if (n>65536u || total>65536u-n) return NULL;
        total+=n;
    }
    char *joined=(char*)malloc(total+1);
    if (!joined) return NULL;
    size_t off=0;
    for (GLsizei i=0;i<count;i++) {
        size_t n=(lengths && lengths[i]>=0)?(size_t)lengths[i]:strlen(strings[i]);
        memcpy(joined+off,strings[i],n);off+=n;
    }
    joined[total]=0;if(out_n)*out_n=total;return joined;
}
#ifdef ZOMBIE_DEBUG_BUILD
static uint64_t shader_dump_hash(const void *data,size_t n) {
    const unsigned char *p=(const unsigned char*)data;
    uint64_t h=1469598103934665603ull;
    while(n--) h=(h^*p++)*1099511628211ull;
    return h;
}
static void shader_dump_glsl(const char *source,size_t n) {
    if (!source || !n) return;
    const char *stage=strstr(source,"gl_Position")?"vs":"fs";
    uint64_t hash=shader_dump_hash(source,n);
    char path[256];
    snprintf(path,sizeof(path),DATA_PATH "glsl_dump/%016llX_%s.glsl",(unsigned long long)hash,stage);
    SceIoStat st;
    if (sceIoGetstat(path,&st)>=0) return;
    if (!file_mkpath(DATA_PATH "glsl_dump/.probe",0777)) return;
    FILE *f=fopen(path,"wb");
    if (!f) return;
    size_t wrote=fwrite(source,1,n,f);fclose(f);
    if (wrote==n) shader_sources_dumped++;
}
#endif

void glUseProgram_soloader(GLuint program) {
    shader_use_calls++;
    if (program==shader_current_program && shader_program_known(program)) {
        shader_use_skips++;return;
    }
    glUseProgram(program);
    if (shader_program_known(program)) shader_current_program=program;
    else shader_current_program=~0u;
}
void glDeleteProgram_soloader(GLuint program) {
    shader_lookup_invalidate(program);shader_program_set_known(program,0);glDeleteProgram(program);
}
GLint glGetUniformLocation_soloader(GLuint program,const GLchar *name) {
    shader_uniform_calls++;return shader_lookup_get(shader_uniform_cache,program,name,0);
}
GLint glGetAttribLocation_soloader(GLuint program,const GLchar *name) {
    shader_attrib_calls++;return shader_lookup_get(shader_attrib_cache,program,name,1);
}
'''


def patch_glutil(text: str) -> str:
    if MARKER not in text:
        anchor = "void glFinish_soloader(void) {\n    uint64_t start = sceKernelGetProcessTimeWide();\n    glFinish(); gl_time(&gl_perf.finish, start);\n}\n"
        if text.count(anchor) != 1:
            raise SystemExit("glutil.c: glFinish anchor drifted")
        text = text.replace(anchor, anchor + runtime + "\n", 1)

    old_link = "void glLinkProgram_soloader(GLuint program) {\n    uint64_t start = sceKernelGetProcessTimeWide();\n    glLinkProgram(program); gl_time(&gl_perf.link, start);\n}\n"
    new_link = "void glLinkProgram_soloader(GLuint program) {\n    shader_lookup_invalidate(program);\n    uint64_t start = sceKernelGetProcessTimeWide();\n    glLinkProgram(program); gl_time(&gl_perf.link, start);\n    GLint ok=GL_FALSE;glGetProgramiv(program,GL_LINK_STATUS,&ok);\n    shader_program_set_known(program,ok==GL_TRUE);\n}\n"
    if new_link not in text:
        if text.count(old_link) != 1:
            raise SystemExit("glutil.c: glLinkProgram anchor drifted")
        text = text.replace(old_link,new_link,1)

    old_source = "void glShaderSource_soloader(GLuint shader,GLsizei count,const GLchar **strings,const GLint *lengths) {\n    /* Native vitaGL already handles concatenation and negative lengths. */\n    glShaderSource(shader,count,strings,lengths);\n}\n"
    new_source = r'''void glShaderSource_soloader(GLuint shader,GLsizei count,const GLchar **strings,const GLint *lengths) {
    size_t n=0;char *joined=shader_concat_source(count,strings,lengths,&n);
#ifdef ZOMBIE_DEBUG_BUILD
    if (joined) shader_dump_glsl(joined,n);
#endif
    if (joined && shader_plain_texture_duplicate(joined,n)) {
        const GLchar *canonical=zombie_plain_texture_fs;
        GLint canonical_len=(GLint)(sizeof(zombie_plain_texture_fs)-1);
        glShaderSource(shader,1,&canonical,&canonical_len);
        shader_sources_deduped++;
    } else {
        glShaderSource(shader,count,strings,lengths);
    }
    free(joined);
}
'''
    if new_source not in text:
        if text.count(old_source) != 1:
            raise SystemExit("glutil.c: glShaderSource anchor drifted")
        text = text.replace(old_source,new_source,1)

    report_anchor = '    l_perf("shader_cache hits=%u misses=%u invalid=%u bytes_loaded=%u compile_us=%u load_us=%u writes_failed=%u actual_compile_calls=%u writes=%u counters=lifetime",stats[0],stats[1],stats[2],stats[3],stats[4],stats[5],stats[6],stats[7],stats[8]);\n'
    report_add = report_anchor + '''    l_perf("shader_runtime use_calls=%u use_skips=%u uniform_calls=%u uniform_hits=%u attrib_calls=%u attrib_hits=%u source_dedupes=%u"
#ifdef ZOMBIE_DEBUG_BUILD
           " dumps=%u"
#endif
           ,shader_use_calls,shader_use_skips,shader_uniform_calls,shader_uniform_hits,
           shader_attrib_calls,shader_attrib_hits,shader_sources_deduped
#ifdef ZOMBIE_DEBUG_BUILD
           ,shader_sources_dumped
#endif
           );
'''
    if "shader_runtime use_calls=" not in text:
        if text.count(report_anchor)!=1:
            raise SystemExit("glutil.c: shader report anchor drifted")
        text=text.replace(report_anchor,report_add,1)
    return text


def patch_dynlib(text: str) -> str:
    proto_anchor = "off_t AAsset_seek_perf(AAsset *, off_t, int);\n"
    protos = proto_anchor + "void glUseProgram_soloader(GLuint);\nvoid glDeleteProgram_soloader(GLuint);\nGLint glGetUniformLocation_soloader(GLuint,const GLchar *);\nGLint glGetAttribLocation_soloader(GLuint,const GLchar *);\n"
    if "void glUseProgram_soloader(GLuint);" not in text:
        if text.count(proto_anchor)!=1: raise SystemExit("dynlib.c: GL prototype anchor drifted")
        text=text.replace(proto_anchor,protos,1)
    replacements={
        '{ "glDeleteProgram", (uintptr_t)&glDeleteProgram },':'{ "glDeleteProgram", (uintptr_t)&glDeleteProgram_soloader },',
        '{ "glGetAttribLocation", (uintptr_t)&glGetAttribLocation },':'{ "glGetAttribLocation", (uintptr_t)&glGetAttribLocation_soloader },',
        '{ "glGetUniformLocation", (uintptr_t)&glGetUniformLocation },':'{ "glGetUniformLocation", (uintptr_t)&glGetUniformLocation_soloader },',
        '{ "glUseProgram", (uintptr_t)&glUseProgram },':'{ "glUseProgram", (uintptr_t)&glUseProgram_soloader },',
    }
    for old,new in replacements.items():
        if new in text: continue
        if text.count(old)!=1: raise SystemExit(f"dynlib.c: import anchor drifted: {old}")
        text=text.replace(old,new,1)
    return text


g=GLUTIL.read_text();d=DYNLIB.read_text()
g2=patch_glutil(g);d2=patch_dynlib(d)
GLUTIL.write_text(g2);DYNLIB.write_text(d2)
required=[MARKER,"shader_runtime use_calls=","glUseProgram_soloader","zombie_plain_texture_fs"]
if any(x not in g2 for x in required) or '&glUseProgram_soloader' not in d2:
    raise SystemExit("shader runtime optimization patch incomplete")
print("Prepared shader runtime dedupe, lookup cache, redundant-bind skip and Debug GLSL dump")
