#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include "video_reader.hpp"


// Fragment shader program takes three textures:
// Ytex, Utex and Vtex which contain the Y U and V
// components of a video frame respectively, and
// runs the color conversion and returns the RGB
// components.

static const char* FRAG_SHADER_SRC =
    "uniform sampler2D Ytex;\n"
    "uniform sampler2D Utex,Vtex;\n"
    "void main(void) {\n"
    "  float r,g,b,y,u,v;\n"
    "  vec2 pos = gl_TexCoord[0].xy;\n"
    "  y = texture2D(Ytex, pos).r;\n"
    "  u = texture2D(Utex, pos).r;\n"
    "  v = texture2D(Vtex, pos).r;\n"

    "  y = 1.1643 * (y - 0.0625);\n"
    "  u = u - 0.5;\n"
    "  v = v - 0.5;\n"

    "  r = y + 1.5958 * v;\n"
    "  g = y - 0.39173 * u - 0.81290 * v;\n"
    "  b = y + 2.017 * u;\n"

    "  gl_FragColor = vec4(r, g, b, 1.0);\n"
    "}\n"
;

static void init_hwaccel_yuv_2_rgb(GLuint* tex_y, GLuint* tex_u, GLuint* tex_v) {

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
    GLuint tex_handles[3] = { GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2 };
    GLint uniform_locs[3] = {
        glGetUniformLocation(program, "Ytex"),
        glGetUniformLocation(program, "Utex"),
        glGetUniformLocation(program, "Vtex")
    };
    for (int i = 0; i < 3; ++i) {
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
    *tex_u = tex_handles[1];
    *tex_v = tex_handles[2];

}

int main(int argc, const char** argv) {
    GLFWwindow* window;

    if (!glfwInit()) {
        printf("Couldn't init GLFW\n");
        return 1;
    }

    window = glfwCreateWindow(800, 480, "Hello World", NULL, NULL);
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

    GLuint tex_y, tex_u, tex_v;
    init_hwaccel_yuv_2_rgb(&tex_y, &tex_u, &tex_v);

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
            glfwWaitEventsTimeout(pt_in_seconds - glfwGetTime());
        }

        // Transfer Y U and V data to the GPU
        glBindTexture(GL_TEXTURE_2D, tex_y);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame_width,   frame_height,   0, GL_RED, GL_UNSIGNED_BYTE, vr_state.av_frame->data[0]);
        glBindTexture(GL_TEXTURE_2D, tex_u);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame_width/2, frame_height/2, 0, GL_RED, GL_UNSIGNED_BYTE, vr_state.av_frame->data[1]);
        glBindTexture(GL_TEXTURE_2D, tex_v);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, frame_width/2, frame_height/2, 0, GL_RED, GL_UNSIGNED_BYTE, vr_state.av_frame->data[2]);

        // Render whatever you want
        glBegin(GL_QUADS);
            glTexCoord2d(0,0); glVertex2i(200, 200);
            glTexCoord2d(1,0); glVertex2i(200 + frame_width, 200);
            glTexCoord2d(1,1); glVertex2i(200 + frame_width, 200 + frame_height);
            glTexCoord2d(0,1); glVertex2i(200, 200 + frame_height);
        glEnd();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    video_reader_close(&vr_state);

    return 0;
}

