#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cglm/cglm.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

enum { SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600 };
enum { MAX_VERT = 1024, MAX_IDX = 4096 };
enum { ONE_MB = 1024 * 1024 };

typedef struct Texture {
    uint32_t id;
    float w, h;
} Texture;

typedef struct Rect {
    float x, y;
    float w, h;
} Rect;

typedef struct Window_State {
    int w, h;
    GLFWwindow *glfw_window;
} Window_State;

typedef struct Gl_State {
    uint32_t vbo;
    uint32_t ebo;
    uint32_t vao;
    uint32_t shader;
    Texture empty_texture;
} Gl_State;

typedef struct {
    Texture tex;
    uint32_t tile_dim;
    uint32_t h_count;
    uint32_t v_count;
} Ascii_Atlas;

static Gl_State g_gl_state;
static char gl_error_buffer[ONE_MB];
static Window_State g_window_state;

void exit_with_error(const char *msg, ...);
void trace_log(const char *msg, ...);
void *xmalloc(size_t bytes);
void *xcalloc(size_t bytes);

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void window_size_callback(GLFWwindow *window, int width, int height);

void print_opengl_debug_info();

uint32_t build_shader_from_src(const char *src, GLenum shader_type);
uint32_t link_vert_frag_shaders(uint32_t vert, uint32_t frag);
uint32_t build_default_shaders();

Gl_State initialize_gl_state();
void set_ortho_projection(int width, int height);

Texture load_texture(const char *file);
Texture load_empty_texture();

void draw_texture(Rect dest, Texture texture, Rect src, vec4 color);
void draw_texture_scaled(vec2 pos, Texture texture, float scale);
void draw_texture_scaled_tinted(vec2 pos, Texture texture, float scale, vec4 color);
void draw_quad(Rect quad, vec4 color);
void draw_ascii_tile(vec2 pos, char glyph, vec4 col, Ascii_Atlas atlas);

int main() {
    if (!glfwInit()) {
        exit_with_error("Failed to initialize GLFW");
    }

    trace_log("GLFW initialized");

    // Set GLFW OpenGL version -- 4.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window_state.w = SCREEN_WIDTH;
    g_window_state.h = SCREEN_HEIGHT;
    g_window_state.glfw_window = glfwCreateWindow(g_window_state.w, g_window_state.h, "ASCII Fluid Dynamics Proto", NULL, NULL);
    if (g_window_state.glfw_window == NULL) {
        exit_with_error("Failed to create GLFW window");
    }

    trace_log("GLFW window created");

    glfwMakeContextCurrent(g_window_state.glfw_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        exit_with_error("Failed to load GL function pointers");
    }

    print_opengl_debug_info();

    glfwSetKeyCallback(g_window_state.glfw_window, keyboard_callback);
    glfwSetWindowSizeCallback(g_window_state.glfw_window, window_size_callback);

    g_gl_state = initialize_gl_state();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, g_window_state.w, g_window_state.h);
    glClearColor(0.09f, 0.07f, 0.07f, 1.0f);

    set_ortho_projection(g_window_state.w, g_window_state.h);

    Texture claesz = load_texture("res/claesz.png");
    Ascii_Atlas curses_atlas = {0};
    curses_atlas.tex = load_texture("res/curses.png");
    curses_atlas.tile_dim = 24;
    curses_atlas.h_count = curses_atlas.tex.w / curses_atlas.tile_dim;
    curses_atlas.v_count = curses_atlas.tex.h / curses_atlas.tile_dim;

    trace_log("Entering main loop");
    while (!glfwWindowShouldClose(g_window_state.glfw_window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        float bg_scale = 0.7f;
        vec2 bg_pos = {
            g_window_state.w * 0.5f - claesz.w * bg_scale * 0.5f,
            g_window_state.h * 0.5f - claesz.h * bg_scale * 0.5f
        };
        draw_texture_scaled_tinted(bg_pos, claesz, bg_scale, (vec4){0.22f, 0.2f, 0.2f, 0.5f});

        draw_texture_scaled((vec2){100.0f, 100.0f}, curses_atlas.tex, 1.0f);

        for (int y = 0; y < 50; y++)
            for (int x = 0; x < 50; x++)
                draw_ascii_tile((vec2){(float)x * curses_atlas.tile_dim, (float)y * curses_atlas.tile_dim},
                                (char)((x + y * curses_atlas.h_count) % 128),
                                (vec4){1.0f, 0.0f, 1.0f, 1.0f},
                                curses_atlas);

        glfwSwapBuffers(g_window_state.glfw_window);
        glfwPollEvents();
    }

    trace_log("GLFW terminating gracefully");

    glfwTerminate();
    return 0;
}

void exit_with_error(const char *msg, ...) {
    fprintf(stderr, "FATAL: ");
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    glfwTerminate();
    exit(1);
}

void trace_log(const char *msg, ...) {
    printf("INFO: ");
    va_list ap;
    va_start(ap, msg);
    vprintf(msg, ap);
    va_end(ap);
    printf("\n");
}

void *xmalloc(size_t bytes) {
    void *d = malloc(bytes);
    if (d == NULL) exit_with_error("Failed to malloc");
    return d;
}
void *xcalloc(size_t bytes) {
    void *d = calloc(1, bytes);
    if (d == NULL) exit_with_error("Failed to calloc");
    return d;
}

void keyboard_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)window; (void)key; (void)scancode; (void)action; (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        trace_log("Received ESC. Terminating...");
        glfwSetWindowShouldClose(window, true);
    }
}

void window_size_callback(GLFWwindow *window, int width, int height) {
    (void)window;

    g_window_state.w = width;
    g_window_state.h = height;
    glViewport(0, 0, width, height);
    set_ortho_projection(width, height);
}

void print_opengl_debug_info() {
    trace_log("Loaded OpenGL function pointers. Debug info:");
    trace_log("  Version:  %s", glGetString(GL_VERSION));
    trace_log("  GLSL:     %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    trace_log("  Vendor:   %s", glGetString(GL_VENDOR));
    trace_log("  Renderer: %s", glGetString(GL_RENDERER));
}

uint32_t build_shader_from_src(const char *src, GLenum shader_type) {
    uint32_t id = glCreateShader(shader_type);
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(id, ONE_MB, NULL, gl_error_buffer);
        exit_with_error("Failed to compile shader (type 0x%04X). Error:\n  %s\nSource:\n%s\n", shader_type, gl_error_buffer, src);
    }

    return id;
}

uint32_t link_vert_frag_shaders(uint32_t vert, uint32_t frag) {
    uint32_t id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);

    int success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(id, ONE_MB, NULL, gl_error_buffer);
        exit_with_error("Failed to compile program. Error:\n  %s", gl_error_buffer);
    }

    return id;
}

uint32_t build_default_shaders() {
    static const char *vert_shader_source =
        "#version 430 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "layout (location = 2) in vec4 aColor;\n"
        "uniform mat4 projection;\n"
        "out vec2 TexCoord;\n"
        "out vec4 Color;\n"
        "void main() {\n"
        "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "    Color = aColor;\n"
        "}";
    uint32_t vert_shader = build_shader_from_src(vert_shader_source, GL_VERTEX_SHADER);

    static const char *frag_shader_source =
        "#version 430 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "in vec4 Color;\n"
        "uniform sampler2D texture1;\n"
        "void main() {\n"
        "    FragColor = Color * texture(texture1, TexCoord);\n"
        "}";
    uint32_t frag_shader = build_shader_from_src(frag_shader_source, GL_FRAGMENT_SHADER);

    uint32_t shader_program = link_vert_frag_shaders(vert_shader, frag_shader);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return shader_program;
}

Gl_State initialize_gl_state() {
    Gl_State gl_state = {0};

    glGenVertexArrays(1, &gl_state.vao);
    glGenBuffers(1, &gl_state.vbo);
    glGenBuffers(1, &gl_state.ebo);

    glBindVertexArray(gl_state.vao);

    glBindBuffer(GL_ARRAY_BUFFER, gl_state.vbo);

    size_t total_size = MAX_VERT * (2 + 2 + 4) * sizeof(float);
    glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_state.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_IDX * sizeof(uint32_t), NULL, GL_DYNAMIC_DRAW);

    // Positions -- vec2
    size_t stride = 2 * sizeof(float);
    size_t offset = 0;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)offset);
    glEnableVertexAttribArray(0);

    // TexCoords -- vec2
    offset += stride * MAX_VERT;
    stride = 2 * sizeof(float);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)offset);
    glEnableVertexAttribArray(1);

    // Color -- vec4
    offset += stride * MAX_VERT;
    stride = 4 * sizeof(float);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void *)offset);
    glEnableVertexAttribArray(2);

    offset += stride * MAX_VERT;
    assert(offset == total_size);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    gl_state.shader = build_default_shaders();

    gl_state.empty_texture = load_empty_texture();

    return gl_state;
}

void set_ortho_projection(int width, int height) {
    mat4 projection;
    glm_ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f, projection);

    glUseProgram(g_gl_state.shader);
    glUniformMatrix4fv(glGetUniformLocation(g_gl_state.shader, "projection"), 1, GL_FALSE, (float *)projection);
    glUseProgram(0);
}

Texture load_texture(const char *file) {
    Texture texture = {0};

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(false);
    int width, height, channel_count;
    uint8_t *image_data = stbi_load(file, &width, &height, &channel_count, STBI_rgb_alpha);
    if (image_data == NULL) {
        exit_with_error("Failed to load image at %s", file);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(image_data);

    texture.w = (float)width;
    texture.h = (float)height;

    return texture;
}

Texture load_empty_texture() {
    Texture texture = {0};
    texture.w = texture.h = 1;

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    uint32_t white = -1;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

void draw_texture(Rect dest, Texture texture, Rect src, vec4 color) {
    glBindBuffer(GL_ARRAY_BUFFER, g_gl_state.vbo);

    size_t total_size = MAX_VERT * (2 + 2 + 4) * sizeof(float);
    size_t vert_to_sub_count = 4;
    size_t idx_to_sub_count = 6;
    assert(vert_to_sub_count < MAX_VERT);
    assert(idx_to_sub_count < MAX_IDX);
    size_t stride, offset;

    // Positions -- vec2
    offset = 0;
    stride = 2 * sizeof(float);
    float positions[] = {
        dest.x, dest.y,
        dest.x + dest.w, dest.y,
        dest.x, dest.y + dest.h,
        dest.x + dest.w, dest.y + dest.h
    };
    assert(sizeof(positions) == stride * vert_to_sub_count);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(positions), positions);

    // TexCoords -- vec2
    offset += stride * MAX_VERT;
    stride = 2 * sizeof(float);
    Rect src_norm = (Rect){src.x / texture.w, src.y / texture.h, src.w / texture.w, src.h / texture.h};
    float tex_coords[] = {
        src_norm.x, src_norm.y,
        src_norm.x + src_norm.w, src_norm.y,
        src_norm.x, src_norm.y + src_norm.h,
        src_norm.x + src_norm.w, src_norm.y + src_norm.h
    };
    assert(sizeof(tex_coords) == stride * vert_to_sub_count);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(tex_coords), tex_coords);

    // Color -- vec4
    offset += stride * MAX_VERT;
    stride = 4 * sizeof(float);
    float colors[16];
    for (int i = 0; i < 4; i++) memcpy(colors + i * 4, color, 4 * sizeof(float));
    assert(sizeof(colors) == stride * vert_to_sub_count);
    glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(colors), colors);

    offset += stride * MAX_VERT;
    assert(offset == total_size);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gl_state.ebo);

    uint32_t indices[] = {
        2, 1, 0,
        2, 3, 1
    };
    assert(sizeof(indices) / sizeof(indices[0]) == idx_to_sub_count);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glUseProgram(g_gl_state.shader);
    glBindVertexArray(g_gl_state.vao);
    glBindTexture(GL_TEXTURE_2D, texture.id);

    glDrawElements(GL_TRIANGLES, idx_to_sub_count, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void draw_texture_scaled(vec2 pos, Texture texture, float scale) {
    vec2 size = {texture.w, texture.h};
    glm_vec2_scale(size, scale, size);
    draw_texture((Rect){pos[0], pos[1], size[0], size[1]},
                 texture,
                 (Rect){0.0f, 0.0f, texture.w, texture.h},
                 (vec4){1.0f, 1.0f, 1.0f, 1.0f});
}

void draw_texture_scaled_tinted(vec2 pos, Texture texture, float scale, vec4 color) {
    vec2 size = {texture.w, texture.h};
    glm_vec2_scale(size, scale, size);
    draw_texture((Rect){pos[0], pos[1], size[0], size[1]},
                 texture,
                 (Rect){0.0f, 0.0f, texture.w, texture.h},
                 color);
}

void draw_quad(Rect quad, vec4 color) {
    draw_texture(quad, g_gl_state.empty_texture, (Rect){0}, color);
}

void draw_ascii_tile(vec2 pos, char glyph, vec4 col, Ascii_Atlas atlas) {
    /* uint32_t h_count = atlas.tex.w / atlas.tile_dim; */

    uint32_t x = (unsigned)glyph % atlas.h_count;
    uint32_t y = (unsigned)glyph / atlas.h_count;

    float x_min = (float)(x * atlas.tile_dim);
    float y_min = (float)(y * atlas.tile_dim);
    float t_d = (float)atlas.tile_dim;

    draw_texture((Rect){pos[0], pos[1], t_d, t_d},
                 atlas.tex,
                 (Rect){x_min, y_min, t_d, t_d},
                 col);
}
