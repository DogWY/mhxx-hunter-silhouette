// macOS offscreen GPU test. Run from the repository root.
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint shader(GLenum type, const char *source) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &source, NULL);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(id, sizeof log, NULL, log);
        fprintf(stderr, "%s\n", log);
        exit(1);
    }
    return id;
}

int main(int argc, char **argv) {
    int native = argc == 2 && strcmp(argv[1], "--native") == 0;
    const CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core, 0
    };
    CGLPixelFormatObj format;
    CGLContextObj context;
    GLint count;
    if (CGLChoosePixelFormat(attrs, &format, &count) || !count ||
        CGLCreateContext(format, NULL, &context) || CGLSetCurrentContext(context)) {
        fprintf(stderr, "CGL context unavailable\n");
        return 1;
    }
    char fragment[8192] = {0};
    FILE *file = fopen("src/occluded_edge.frag", "rb");
    if (!file) return 1;
    size_t length = fread(fragment, 1, sizeof fragment, file);
    if (!length || length == sizeof fragment || ferror(file)) return 1;
    fclose(file);
    GLuint program = glCreateProgram();
    const char *vertex = "#version 410 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2, gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}";
    char native_vertex[2048] = {0};
    if (native) {
        char *body = strchr(fragment, '\n') + 1;
        const char flag[] = "#define TEST_NATIVE_DEPTH 1\n";
        if (length + sizeof flag >= sizeof fragment) return 1;
        memmove(body + sizeof flag - 1, body, strlen(body) + 1);
        memcpy(body, flag, sizeof flag - 1);
        file = fopen("src/fullscreen.vert", "rb");
        if (!file || !fread(native_vertex, 1, sizeof native_vertex - 1, file)) return 1;
        fclose(file);
        vertex = native_vertex;
    }
    glAttachShader(program, shader(GL_VERTEX_SHADER, vertex));
    glAttachShader(program, shader(GL_FRAGMENT_SHADER, fragment));
    glLinkProgram(program);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) return 1;
    glUseProgram(program);
    enum { width = 15, height = 13, pixels = width * height };
    GLuint target, fbo, vao, layers[3];
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 1;
    glViewport(0, 0, width, height);
    glGenTextures(3, layers);
    const char *names[] = {"hunterDepth", "largeMonsterDepth", "sceneDepth"};
    glUniform1f(glGetUniformLocation(program, "depthTolerance"), 0.001f);
    for (int test = 0; test < (native ? 12 : 18) * 8; ++test) {
        int radius = 1 + test / (native ? 12 : 18);
        int scenario = (native ? 6 : 0) + test % (native ? 12 : 18);
        glUniform1i(glGetUniformLocation(program, "outlineWidth"), radius);
        int frame = scenario % 6;
        int encoding = scenario / 6;
        // Near 1, infinite far. Reverse Z stores 1/z, forward Z stores 1-1/z.
        float clear = encoding == 1 ? 1.0f : 0.0f;
        const float decode[][4] = {{1,0,0,1}, {0,1,-1,1}, {0,1,1,0}};
        glUniform1f(glGetUniformLocation(program, "clearDepth"), clear);
        glUniform4fv(glGetUniformLocation(program, "depthDecode"), 1, decode[encoding]);
        float depth[3][pixels];
        unsigned char expected[pixels], rgba[pixels][4];
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            int i = width*y+x;
            int hunter = x >= 2 && x <= 12 && y >= 2 && y <= 10;
            int monster = frame == 2 || frame == 3 || frame == 4 ||
                          (frame == 1 && y >= 6);
            depth[0][i] = hunter ? 5 : 0;
            depth[1][i] = monster ? (frame == 4 ? 7 : 3) : 0;
            depth[2][i] = frame == 3 ? 1 : monster && frame != 4 ? 3 : hunter ? 5 : 10;
            expected[i] = hunter && (x < 2 + radius || x > 12 - radius || y < 2 + radius || y > 10 - radius) &&
                          (frame == 2 || (frame == 1 && y >= 6));
        }
        for (int i = 0; i < 3; ++i) {
            if (encoding) {
                for (int j = 0; j < pixels; ++j) {
                    float distance = depth[i][j];
                    if (!distance) depth[i][j] = clear;
                    else if (encoding == 1) depth[i][j] = 1.0f - 1.0f / distance;
                    else depth[i][j] = 1.0f / distance;
                }
            }
            glActiveTexture(GL_TEXTURE0+i);
            glBindTexture(GL_TEXTURE_2D, layers[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, depth[i]);
            glUniform1i(glGetUniformLocation(program, names[i]), i);
        }
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, native ? (radius-1)*8 + (clear == 1.0f ? 4 : 0) : 0, 3);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if (glGetError() != GL_NO_ERROR) return 1;
        for (int i = 0; i < pixels; ++i) if ((rgba[i][3] != 0) != expected[i]) {
            fprintf(stderr, "Incorrect edge at scenario %d pixel %d\n", scenario, i);
            return 1;
        }
    }
    puts(native ? "All 96 native depth scenarios passed." : "All 144 offscreen GPU depth scenarios passed.");
    return 0;
}
