#include <cstdint>
#include <format>
#include <print>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <iostream>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include "camera.hpp"
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/implot.h>

static constexpr glm::vec3 vertices[] {
    glm::vec3{-1.0, -1.0,  1.0},
    glm::vec3{ 1.0, -1.0,  1.0},
    glm::vec3{ 1.0,  1.0,  1.0},
    glm::vec3{-1.0,  1.0,  1.0},
    glm::vec3{-1.0, -1.0, -1.0},
    glm::vec3{ 1.0, -1.0, -1.0},
    glm::vec3{ 1.0,  1.0, -1.0},
    glm::vec3{-1.0,  1.0, -1.0},

    // PLANE
    glm::vec3{-1.0, -1.0, 1.0},
    glm::vec3{ 1.0, -1.0, 1.0},
    glm::vec3{-1.0,  1.0, 1.0},
    glm::vec3{ 1.0,  1.0, 1.0},

    // TRIANGLE
    glm::vec3{-1.0, -1.0, 0.0},
    glm::vec3{ 1.0, -1.0, 0.0},
    glm::vec3{ 0.0,  1.0, 0.0},
};

static constexpr uint32_t indices[] {
    0, 1, 2,
    2, 3, 0,
    /*top*/
    1, 5, 6,
    6, 2, 1,
    /*back*/
    7, 6, 5,
    5, 4, 7,
    /*bottom*/
    4, 0, 3,
    3, 7, 4,
    /*left*/
    4, 5, 1,
    1, 0, 4,
    /*right*/
    3, 2, 6,
    6, 7, 3,


    // PLANE
     8, 9, 10, 
    10, 9, 11,

    // Triangle
    12, 13, 14
};

static constexpr auto vertex_count = sizeof(vertices) / sizeof(vertices[0]);
static constexpr auto index_count = sizeof(indices) / sizeof(indices[0]);

static constexpr auto default_vertex_str = R"glsl(
    #version 460 core

    layout(location=0) in vec3 pos;

    layout(location=5) flat out int instance;

    uniform mat4 P;
    uniform mat4 V;
    uniform float time;

    const float scale = 0.2;

    void main() {
        instance = gl_BaseInstance + gl_InstanceID;
        vec3 wp = pos * scale;
        
        if(instance < 20) {
            float s = sin(time * 0.5 + float(instance) / 20.0 * 3.141592);
            float c = cos(time * 0.5 + float(instance) / 20.0 * 3.141592);
            wp = mat3(c, 0, -s, 0, 1, 0, s, 0, c) * wp;
            wp += vec3(1.0, 1.0, 1.0) * instance * scale;
        } else {
            if(instance == 21) {
                const float s = sin(3.141592 * -0.5);
                const float c = cos(3.141592 * -0.5);
                wp = mat3(1, 0, 0, 0, c, s, 0, -s, c) * wp;
            }
            wp += vec3(-1.0, 0.0, 0.0);
        }

        vec4 projected_pos = P * (V * vec4(wp, 1.0));
        gl_Position = projected_pos;
    }
)glsl";

static constexpr auto default_fragment_str = R"glsl(
    #version 460 core

    out vec4 FRAG_COLOR;

    layout(location=5) flat in int instance;

    layout(r32f, binding=0) uniform image2D zbuffer;
    uniform float time;
    uniform int use_buffer;

    void main() {
        ivec2 screen_pos = ivec2(gl_FragCoord.xy);
        float stored_z = imageLoad(zbuffer, screen_pos).r;

        if(use_buffer == 1) {
            if(stored_z <= gl_FragCoord.z) { discard; }
            imageStore(zbuffer, screen_pos, vec4(gl_FragCoord.z));
        }

        float s = pow(sin(float(instance) / 20.0 * 3.141592 + time * 0.2), 2.0);
        float c = pow(cos(float(instance) / 20.0 * 3.141592 + time * 0.2), 2.0);
        FRAG_COLOR = vec4(vec3(s, c, 0.3*s+0.7*c), 1.0);

        if(instance == 20) {
            FRAG_COLOR = vec4(1.0); 
        } else if (instance == 21) {
            FRAG_COLOR = vec4(vec3(0.4), 1.0);
        }
    }
)glsl";

static constexpr auto clear_zbuffer_vertex_str = R"glsl(
    #version 460 core

    layout(location=0) in vec3 pos;

    void main() {
        gl_Position = vec4(pos, 1.0);
    }
)glsl";

static constexpr auto clear_zbuffer_fragment_str = R"glsl(
    #version 460 core

    layout(location=0) out float FRAG_COLOR0;
    
    void main() {
        FRAG_COLOR0 = 1.0;
    }
)glsl";

static constexpr auto show_zbuffer_vertex_str = R"glsl(
    #version 460 core

    layout(location=0) in vec3 pos;

    void main() {
        gl_Position = vec4(pos, 1.0);
    }
)glsl";

static constexpr auto show_zbuffer_fragment_str = R"glsl(
    #version 460 core

    layout(location=0) out vec4 FRAG_COLOR0;
    layout(r32f, binding=0) uniform image2D zbuffer;

    void main() {
        const float f = 30.0;
        const float n = 0.01;

        float z = imageLoad(zbuffer, ivec2(gl_FragCoord.xy)).r;
        z = z*2.0 - 1.0;
        z = (2.0 * n * f) / (f + n - z * (f - n)) / f;

        FRAG_COLOR0 = vec4(z, z, z, 1.0);
    }
)glsl";

static std::vector<std::byte> read_file(const char* path) {
    std::ifstream file{path};    

    if(!file) { return {}; }

    file.seekg(0, std::ios::end);
    std::vector<std::byte> content(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(reinterpret_cast<char*>(content.data()), content.size());

    return content;
}

static uint32_t make_program(const std::byte* vertex_source, const std::byte* fragment_source) {
    uint32_t vertex = glCreateShader(GL_VERTEX_SHADER);
    uint32_t fragment = glCreateShader(GL_FRAGMENT_SHADER);
    uint32_t program = glCreateProgram();

    glShaderSource(vertex, 1, (const char**)&vertex_source, nullptr);
    glShaderSource(fragment, 1, (const char**)&fragment_source, nullptr);

    static constexpr auto compile_shader = [](uint32_t shader) {
        glCompileShader(shader);

        int compile_status;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

        if(compile_status == GL_FALSE) {
            char buffer[512] = {};
            int length;
            glGetShaderInfoLog(shader, 512, &length, buffer);
            std::println("Shader compilation error: {}", buffer);

            return false;
        }

        return true;
    };

    bool status = true;
    status &= compile_shader(vertex);
    status &= compile_shader(fragment);

    if(!status) { return 0; }

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    glLinkProgram(program);

    int link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);

    if(!link_status) {
        char buffer[512] = {};
        int length;
        glGetProgramInfoLog(program, 512, &length, buffer);
        std::println("Shader program link error: {}", buffer);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static void try_compile_program(uint32_t& program, const std::byte* vertex_source, const std::byte* fragment_source) {
    if(auto new_prog = make_program(vertex_source, fragment_source); new_prog != 0) {
        program = new_prog;
    }
}

int main() {
    static constexpr auto window_width = 1280.0f;
    static constexpr auto window_height = 768.0f;
    static constexpr auto window_aspect = window_width / window_height;

    glfwInit();    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    auto* window  = glfwCreateWindow(window_width, window_height, "brdf", 0, 0);

    if(!window) {
        std::println("Window could not be created");
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::println("Glad could not load gl functions");
        return -1;
    }

    gp::Camera camera{window, glm::pi<float>() / 2.0f, window_aspect};

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = "imgui.ini";
    io.IniSavingRate = 5.0f;

    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");
    ImPlot::CreateContext();

    uint32_t default_program, clear_buffer_program, show_buffer_program;
    try_compile_program(default_program, (const std::byte*)default_vertex_str, (const std::byte*)default_fragment_str);
    try_compile_program(clear_buffer_program, (const std::byte*)clear_zbuffer_vertex_str, (const std::byte*)clear_zbuffer_fragment_str);
    try_compile_program(show_buffer_program, (const std::byte*)show_zbuffer_vertex_str, (const std::byte*)show_zbuffer_fragment_str);

    uint32_t zbuffer;
    glCreateTextures(GL_TEXTURE_2D, 1, &zbuffer);
    glTextureStorage2D(zbuffer, 1, GL_R32F, window_width, window_height);
    glTextureParameteri(zbuffer, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(zbuffer, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(zbuffer, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(zbuffer, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    uint32_t clear_fbo;
    glCreateFramebuffers(1, &clear_fbo);
    uint32_t draw_buffers[] {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glNamedFramebufferTexture(clear_fbo, GL_COLOR_ATTACHMENT0, zbuffer, 0);
    glNamedFramebufferDrawBuffers(clear_fbo, 1, draw_buffers);
    glNamedFramebufferReadBuffer(clear_fbo, GL_NONE);

    if(glCheckNamedFramebufferStatus(clear_fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::println("Bad clear fbo creation");
        return -1;
    }

    uint32_t vertex_buffer, index_buffer;
    glCreateBuffers(1, &vertex_buffer);
    glCreateBuffers(1, &index_buffer);
    glNamedBufferStorage(vertex_buffer, sizeof(vertices), vertices, 0);
    glNamedBufferStorage(index_buffer, sizeof(indices), indices, 0);

    uint32_t vao;
    glCreateVertexArrays(1, &vao);

    glVertexArrayVertexBuffer(vao, 0, vertex_buffer, 0, 12);
    glVertexArrayElementBuffer(vao, index_buffer);

    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);

    int use_buffer = 1;
    int show_buffer = 0;

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        camera.update();
        if(ImGui::IsKeyPressed(ImGuiKey_1, false)) { use_buffer = 1 - use_buffer; }
        if(ImGui::IsKeyPressed(ImGuiKey_2, false)) { show_buffer = 1 - show_buffer; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        glViewport(0, 0, window_width, window_height);

        glBindVertexArray(vao);
        glUseProgram(clear_buffer_program);
        glBindFramebuffer(GL_FRAMEBUFFER, clear_fbo);
        glDrawElementsInstancedBaseInstance(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)((index_count-9) * sizeof(indices[0])), 1, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const auto V = camera.get_v_mat();
        glUseProgram(default_program);
        glUniformMatrix4fv(glGetUniformLocation(default_program, "P"), 1, GL_FALSE, &camera.perspective[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(default_program, "V"), 1, GL_FALSE, &V[0][0]);
        glUniform1f(glGetUniformLocation(default_program, "time"), glfwGetTime());
        glUniform1i(glGetUniformLocation(default_program, "use_buffer"), use_buffer);
        glBindImageTexture(0, zbuffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

        for(int i=0; i<20; ++i) {
            glDrawElementsInstancedBaseInstance(GL_TRIANGLES, index_count - 9, GL_UNSIGNED_INT, 0, 1, i);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        for(int i=0; i<2; ++i) {
            glDrawElementsInstancedBaseInstance(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (const void*)((index_count-3) * sizeof(indices[0])), 1, i+20);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }

        if(show_buffer == 1) {
            glUseProgram(show_buffer_program);
            glDrawElementsInstancedBaseInstance(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)((index_count-9) * sizeof(indices[0])), 1, 0);
        } 

        ImGui::NewFrame();

        if(ImGui::Begin("depth plot")) {
            float xs2[30], ys2[30];
            auto p = camera.perspective;
            int idx = 0;
            for(int i=0; i<30; ++i) {
                auto v = p * glm::vec4{0.0f, 0.0f, -(float)i, 1.0f};
                v.z /= v.w;
                xs2[idx++] = (float)i;
                ys2[idx-1] = v.z;
            }
            
            if (ImPlot::BeginPlot("Line Plots")) {
                ImPlot::SetupAxes("x","y");
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
                ImPlot::PlotLine("g(x)", xs2, ys2, 30);
                ImPlot::EndPlot();
            }
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}