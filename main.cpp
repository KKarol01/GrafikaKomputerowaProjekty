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
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_glfw.h>

static glm::vec2 to_vec2(ImVec2 vec) { return {vec.x, vec.y}; }

static double scrollY = 0.0;
static void scroll_callback(GLFWwindow* window, double x, double y) {
    scrollY = y;
}

static glm::vec2 mouse_position{0.0f};
static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    mouse_position = {xpos, ypos};
}

namespace gp {
    struct Quaternion {
        constexpr Quaternion() = default;
        Quaternion(float a, glm::vec3 xyz): a(a), x(xyz.x), y(xyz.y), z(xyz.z) { }

        Quaternion conjugate() const {
            return Quaternion{a, -glm::vec3{x, y, z}};
        }

        static Quaternion fromAngleAxis(float angle, glm::vec3 axis) {
            const float h = angle * 0.5f;
            return {glm::cos(h), axis * glm::sin(h)};
        }

        static Quaternion identity() {
            return Quaternion{1.0f, glm::vec3{0.0f}};
        }

        float a, x, y, z;
    };

    Quaternion operator*(const Quaternion& a, const Quaternion& b) {
        const auto vec_a = glm::vec3{a.x, a.y, a.z};
        const auto vec_b = glm::vec3{b.x, b.y, b.z};

        return {
            a.a * b.a - glm::dot(vec_a, vec_b),
            a.a*vec_b + b.a*vec_a + glm::cross(vec_a, vec_b)
        };
    }

    glm::vec3 operator*(const Quaternion& a, const glm::vec3& v) {
        const auto res = a * Quaternion{0.0f, v} * a.conjugate();
        return glm::vec3{res.x, res.y, res.z};
    }
    
    struct Camera {
        Camera(GLFWwindow* window, double fovy, double aspect) : window(window), fovy(fovy), aspect(aspect), last_mouse_pos(mouse_position), last_time(glfwGetTime())
        { change_perspective(fovy, aspect, near, far); }

        void update() {
            if(ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
                if(!hide_cursor) { hide_cursor = true; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }
                else            { hide_cursor = false; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }
            }

            const auto mouse_delta = mouse_position - last_mouse_pos;
            last_mouse_pos = mouse_position;
            const auto mouse_wheel = scrollY;
            const auto delta_time = (float)(glfwGetTime() - last_time);
            last_time = glfwGetTime();

            if(hide_cursor) { return; }

            // std::println("Mouse delta: {} {}, scroll: {}, delta {}", mouse_delta.x, mouse_delta.y, mouse_wheel, delta_time);

            yaw -= mouse_delta.x * mouse_sensitivity * delta_time;
            pitch -= mouse_delta.y * mouse_sensitivity * delta_time;
            pitch = glm::clamp(pitch, -glm::half_pi<float>(), glm::half_pi<float>());

            rotation = Quaternion::fromAngleAxis(yaw, glm::vec3{0.0f, 1.0f, 0.0f});
            rotation = rotation * Quaternion::fromAngleAxis(pitch, glm::vec3{1.0f, 0.0f, 0.0f});

            forward = glm::normalize(rotation * glm::vec3{0.0f, 0.0f, -1.0f});
            right = glm::normalize(rotation * glm::vec3{1.0f, 0.0f, 0.0f});
            up = glm::normalize(glm::angleAxis(roll, forward) * glm::cross(right, forward));

            if(ImGui::IsKeyDown(ImGuiKey_W)) { position += forward * keyboard_sensitivity * delta_time; }
            if(ImGui::IsKeyDown(ImGuiKey_S)) { position -= forward * keyboard_sensitivity * delta_time; }
            if(ImGui::IsKeyDown(ImGuiKey_A)) { position -= right * keyboard_sensitivity * delta_time; }
            if(ImGui::IsKeyDown(ImGuiKey_D)) { position += right * keyboard_sensitivity * delta_time; }
            if(ImGui::IsKeyDown(ImGuiKey_Space))      { position += up * keyboard_sensitivity * delta_time; }
            if(ImGui::IsKeyDown(ImGuiKey_LeftShift))  { position -= up * keyboard_sensitivity * delta_time; }
            if(ImGui::IsKeyDown(ImGuiKey_Q)) { roll -= delta_time * keyboard_sensitivity; }
            if(ImGui::IsKeyDown(ImGuiKey_E)) { roll += delta_time * keyboard_sensitivity; }
            if(glm::abs(mouse_wheel) > 1e-5f) {
                fovy = glm::clamp(fovy + mouse_wheel * delta_time * mouse_wheel_sensitivity, glm::pi<double>() / 6.0, glm::pi<double>() / 2.0);
                change_perspective(fovy, aspect, near, far);
            }
        }

        glm::mat4 get_pv_mat() const {
            return perspective * get_v_mat();
        }

        glm::mat4 get_v_mat() const {
            return glm::lookAt(position, position + forward, up);
        }

        void change_perspective(double fovy, double aspect, double near, double far) {
            const auto S = 1.0f / glm::tan(fovy * 0.5f);
            const auto Z = - far / (far - near);
            perspective = glm::mat4{
                glm::vec4{S, 0,          0,       0},
                glm::vec4{0, S * aspect, 0,       0},
                glm::vec4{0, 0,          Z,      -1},
                glm::vec4{0, 0,          Z*near,  0}
            };
        }

        GLFWwindow* window;
        float yaw{0.0f}, pitch{0.0f}, roll{0.0f};
        double fovy{0.0}, aspect{0.0}, near{0.01}, far{100.0};
        glm::mat4 perspective;
        Quaternion rotation{Quaternion::identity()};
        glm::vec3 position{0.0f};
        glm::vec3 forward{0.0f, 0.0f, -1.0f}, right{1.0f, 0.0f, 0.0f}, up{0.0f, 1.0f, 0.0f};
        float mouse_sensitivity{0.5f};
        float mouse_wheel_sensitivity{20.0f};
        float keyboard_sensitivity{2.0f};
        bool hide_cursor = true;
        glm::vec2 last_mouse_pos;
        double last_time;
    };
}

static constexpr auto default_vertex_str = R"glsl(
    #version 460 core
    layout(location=0) in vec3 pos;
    layout(location=0) out struct VERT_OUT {
        vec3 pos;
    } vert;

    flat out mat4 invP;
    flat out mat4 invV;

    uniform mat4 P;
    uniform mat4 V;

    void main() {
        vert.pos = pos;
        invP = inverse(P);
        invV = inverse(V);
        gl_Position = vec4(pos, 1.0);
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
    glfwWindowHint(GLFW_SAMPLES, 4);

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = "imgui.ini";
    io.IniSavingRate = 5.0f;

    ImGui::StyleColorsDark();
    
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    gp::Camera camera{window, glm::pi<float>() / 2.0f, window_aspect};
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    mouse_position = {xpos, ypos};

    uint32_t default_program;
    auto fragment_shader_vector = read_file("shaders/raytrace.frag");
    try_compile_program(default_program, (const std::byte*)default_vertex_str, fragment_shader_vector.data());

    float vertices[] {
        -1.0f,-1.0f, 0.0f,
         1.0f,-1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
         1.0f, 1.0f, 0.0f
    };
    uint32_t indices[] {0, 1, 2, 1, 2, 3};

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

    struct alignas(16) SceneModel {
        glm::vec4 position;
        uint32_t type;
    };
    struct alignas(16) Material {
        glm::vec4 ambient;
        glm::vec4 diffuse;
        glm::vec4 specular;
    };

    struct Settings {
        uint32_t max_iterations{100};
        uint32_t diffuse_samples{8};
        float prec{0.005f};
        float max_dist{30.0f};
    };

    SceneModel models[] {
        {{-4.0f, 0.0f, 0.0f, 0.0f}, 0},
        {{-2.0f, 0.0f, 0.0f, 1.0f}, 1},
        {{ 0.0f, 0.0f, 0.0f, 1.0f}, 1},
        {{ 2.0f, 0.0f, 0.0f, 1.0f}, 1},
        {{ 4.0f, 0.0f, 0.0f, 1.0f}, 1},
    };

    Material materials[] {
        {
            glm::vec4(0.2f, 0.2f, 0.2f, 1.0f),
            glm::vec4(0.2f, 0.2f, 0.2f, 0.0f),
            glm::vec4(0.2f, 0.2f, 0.2f, 1.0f),
        },
        {
            // Chrome
            glm::vec4(0.25f, 0.25f, 0.25f, 2.9f),
            glm::vec4(0.4f, 0.4f, 0.4f, 1.0f),
            glm::vec4(glm::vec3{0.774597}, 76.8f),
        },
        {
            // Copper
            glm::vec4(0.19125, 0.0735, 0.0225, 1.1f),
            glm::vec4(0.7038, 0.27048, 0.0828, 0.279),
            glm::vec4(0.256777, 0.137622, 0.086014, 12.8f),
        },
        {
            // Wall
            glm::vec4(glm::vec3(65.0f / 255.0f), 1.0f),
            glm::vec4(glm::vec3(145.0f / 255.0f), 0.15f),
            glm::vec4(glm::vec3{0.0f}, 1.0f),
        },
        {
            // Wall specular
            glm::vec4(glm::vec3(65.0f / 255.0f), 4.0f),
            glm::vec4(glm::vec3(145.0f / 255.0f), 0.05f),
            glm::vec4(glm::vec3{1.0f}, 259.0f),
        },
    };

    assert(sizeof(models) / sizeof(models[0]) == (sizeof(materials) / sizeof(materials[0])));
    
    uint32_t models_buffer, materials_buffer, settings_buffer;
    glCreateBuffers(1, &models_buffer);
    glCreateBuffers(1, &materials_buffer);
    glCreateBuffers(1, &settings_buffer);
    glNamedBufferStorage(models_buffer, sizeof(models), models, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glNamedBufferStorage(materials_buffer, sizeof(materials), materials, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glNamedBufferStorage(settings_buffer, sizeof(Settings), 0, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    auto* models_memory = (SceneModel*)glMapNamedBufferRange(models_buffer, 0, sizeof(models), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    auto* materials_memory = (Material*)glMapNamedBufferRange(materials_buffer, 0, sizeof(materials), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    Settings* settings_memory = (Settings*)glMapNamedBufferRange(settings_buffer, 0, sizeof(Settings), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    new(settings_memory)Settings{};

    while(!glfwWindowShouldClose(window)) {
        scrollY = 0.0f;
        glfwPollEvents();
        camera.update();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        glUseProgram(default_program);
        glBindVertexArray(vao);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, models_buffer);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, materials_buffer);
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, settings_buffer);

        const auto V = camera.get_v_mat();
        glUniformMatrix4fv(glGetUniformLocation(default_program, "P"), 1, GL_FALSE, &camera.perspective[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(default_program, "V"), 1, GL_FALSE, &V[0][0]);
        glUniform3fv(glGetUniformLocation(default_program, "camera_pos"), 1, &camera.position.x);
        glUniform1ui(glGetUniformLocation(default_program, "model_count"), 5);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        ImGui::NewFrame();
        ImGui::Begin("asdf");
        if(ImGui::Button("recompile shaders")) {
            fragment_shader_vector = read_file("shaders/raytrace.frag");
            try_compile_program(default_program, (const std::byte*)default_vertex_str, fragment_shader_vector.data());
        }
        ImGui::SliderInt("max iterations", (int*)&settings_memory->max_iterations, 1, 500);
        ImGui::SliderFloat("precision", &settings_memory->prec, 0.0f, 1.0f);
        ImGui::SliderFloat("max distance", &settings_memory->max_dist, 0.0f, 200.0f);
        for(int i=0; i<5; ++i) {
            ImGui::PushID(i);
            if(ImGui::CollapsingHeader(std::to_string(i).c_str())) {
                ImGui::ColorEdit3("ambient", &materials_memory[i].ambient.x);
                ImGui::ColorEdit3("diffuse", &materials_memory[i].diffuse.x);
                ImGui::ColorEdit3("specular", &materials_memory[i].specular.x);
                ImGui::SliderFloat("specular", &materials_memory[i].specular.w, 1.0f, 800.0f);
                ImGui::SliderFloat("ior", &materials_memory[i].ambient.w, 0.0f, 5.0f);
                ImGui::SliderFloat("reflectivity", &materials_memory[i].diffuse.w, 0.0f, 1.0f);
            }
            ImGui::PopID();
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