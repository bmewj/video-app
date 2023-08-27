#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>

#include <assert.h>
#include "video_reader.hpp"

// Fragment shader program takes two textures:
// Ytex and UVtex. Ytex contains the luminance
// component of the video frame, while UVtex
// contains both chrominance components.
//
// This format is known as NV12, and appears to
// be the default output from VideoToolbox when
// decoding H.264 video files.
//
// The internal format of the UVtex is a littel strange,
// in part due to the fact that on the old OpenGL version
// that is supported on Mac I can't use the GL_RG8 format
// for loading texture data.
//
// The alternative I went with was to use the internal
// format GL_LUMINANCE_ALPHA, which takes a 2-byte per
// pixel format and drops the first byte into the r, g, and
// b channel of the texture, and the second byte into the a
// channel of the texture.
//
// Inside the fragment shader I am thus pulling out the U
// value for a pixel by accessing .r and the V value by
// accessing .a

static const char* FRAG_SHADER_SRC =
    "uniform sampler2D Ytex;\n"
    "uniform sampler2D UVtex;\n"
    "void main(void) {\n"
    "  float r,g,b,y,u,v;\n"
    "  vec2 pos = gl_TexCoord[0].xy;\n"
    "  y = texture2D(Ytex, pos).r;\n"
    "  u = texture2D(UVtex, pos).r;\n"
    "  v = texture2D(UVtex, pos).a;\n"

    "  y = 1.1643 * (y - 0.0625);\n"
    "  u = u - 0.5;\n"
    "  v = v - 0.5;\n"

    "  r = y + 1.5958 * v;\n"
    "  g = y - 0.39173 * u - 0.81290 * v;\n"
    "  b = y + 2.017 * u;\n"

    "  gl_FragColor = vec4(r, g, b, 1.0);\n"
    "}\n"
;

static void init_hwaccel_nv12_to_rgb(GLuint* tex_y, GLuint* tex_uv) {

    // Create YUV conversion program, frag_shader & compile
    GLuint program = glCreateProgram();
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &FRAG_SHADER_SRC, NULL);
    glCompileShader(frag_shader);

    {
        constexpr int LEN = 32768;
        char* str = (char*)malloc(LEN);
        GLsizei len;
        glGetShaderInfoLog(frag_shader, LEN, &len, str);
        if (len > 0) {
            printf("Shader compile log: %s\n", str);
        }
        free(str);
    }

    glAttachShader(program, frag_shader);
    glLinkProgram(program);
    {
        constexpr int LEN = 32768;
        char* str = (char*)malloc(LEN);
        GLsizei len;
        glGetProgramInfoLog(program, LEN, &len, str);
        if (len > 0) {
            printf("Program link log: %s\n", str);
        }
        free(str);
    }

    glUseProgram(program);

    // Prepare the three textures
    GLuint tex_handles[2] = { GL_TEXTURE0, GL_TEXTURE1 };
    GLint uniform_locs[2] = {
        glGetUniformLocation(program, "Ytex"),
        glGetUniformLocation(program, "UVtex")
    };
    for (int i = 0; i < 2; ++i) {
        assert(uniform_locs[i] != -1);
        glActiveTexture(tex_handles[i]);
        glUniform1i(uniform_locs[i], i);
        glBindTexture(GL_TEXTURE_2D, tex_handles[i]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    // Return the texture handles
    *tex_y = tex_handles[0];
    *tex_uv = tex_handles[1];

}

int main(int argc, const char** argv) {
    GLFWwindow* window;

    if (!glfwInit()) {
        printf("Couldn't init GLFW\n");
        return 1;
    }

    window = glfwCreateWindow(960, 540, "Hello World", NULL, NULL);
    if (!window) {
        printf("Couldn't open window\n");
        return 1;
    }

    VideoReaderState vr_state;
    if (!video_reader_open(&vr_state, "/Users/bmj/Desktop/SPACE ECHO.mov")) {
        printf("Couldn't open video file (make sure you set a video file that exists)\n");
        return 1;
    }
    int frame_width = vr_state.width;
    int frame_height = vr_state.height;

    glfwMakeContextCurrent(window);

    GLuint tex_y, tex_uv;
    init_hwaccel_nv12_to_rgb(&tex_y, &tex_uv);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Set up orphographic projection
        int window_width, window_height;
        glfwGetFramebufferSize(window, &window_width, &window_height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, window_width, window_height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);

        // Read a new frame and load it into texture
        int64_t pts;
        if (!video_reader_read_frame(&vr_state, &pts)) {
            printf("Couldn't load video frame\n");
            return 1;
        }

        static bool first_frame = true;
        if (first_frame) {
            glfwSetTime(0.0);
            first_frame = false;
        }

        double pt_in_seconds = pts * (double)vr_state.time_base.num / (double)vr_state.time_base.den;
        while (pt_in_seconds > glfwGetTime()) {
            glfwWaitEventsTimeout(0.1 * (pt_in_seconds - glfwGetTime()));
        }

        // Transfer Y and UV data to the GPU
        glBindTexture(GL_TEXTURE_2D, tex_y);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame_width, frame_height, 0, GL_RED, GL_UNSIGNED_BYTE, vr_state.av_frame->data[0]);
        glBindTexture(GL_TEXTURE_2D, tex_uv);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, frame_width/2, frame_height/2, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, vr_state.av_frame->data[1]);

        // Render whatever you want
        glBegin(GL_QUADS);
            glTexCoord2d(0,0); glVertex2i(0, 0);
            glTexCoord2d(1,0); glVertex2i(frame_width, 0);
            glTexCoord2d(1,1); glVertex2i(frame_width, frame_height);
            glTexCoord2d(0,1); glVertex2i(0, frame_height);
        glEnd();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    video_reader_close(&vr_state);

    return 0;
}

