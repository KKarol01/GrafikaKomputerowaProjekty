#include "raylib.h"
#include "raymath.h"
#define RAYGUI_IMPLEMENTATION 
#include "raygui.h"
#include <cstdint>
#include <format>
#include <print>
#include <cstdio>
#include <iostream>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

static constexpr glm::vec3 cube_verts[] {
    glm::vec3{-1, -1, -1},    /* 0 Front Bottom Left */
    glm::vec3{ 1, -1, -1},    /* 1 Front Bottom Right */
    glm::vec3{-1, -1,  1},    /* 2 Back Bottom Left */
    glm::vec3{ 1, -1,  1},    /* 3 Back Bottom Right */
    glm::vec3{-1,  1, -1},    /* 4 Front Top Left */
    glm::vec3{ 1,  1, -1},    /* 5 Front Top Right */
    glm::vec3{-1,  1,  1},    /* 6 Back Top Left */
    glm::vec3{ 1,  1,  1},    /* 7 Back Top Right */
};

static constexpr uint32_t cube_wire_frame[] {
    0, 1, 1, 5, 5, 4, 4, 0, // Front face
    2, 3, 3, 7, 7, 6, 6, 2, // Back face
    
    4, 6,
    5, 7,
    0, 2,
    1, 3,
};

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
        Camera(double fovy, double aspect) : fovy(fovy), aspect(aspect)
        { change_perspective(fovy, aspect, near, far); }

        void update() {
            if(IsKeyPressed(KEY_TAB)) {
                if(IsCursorHidden()) { EnableCursor(); }
                else                 { DisableCursor(); }
            }
            if(!IsCursorHidden()) { return; }

            const auto mouse_position = GetMousePosition();
            const auto mouse_delta = GetMouseDelta();
            const auto mouse_wheel = GetMouseWheelMove();
            const auto delta_time = GetFrameTime();

            yaw -= mouse_delta.x * mouse_sensitivity * delta_time;
            pitch += mouse_delta.y * mouse_sensitivity * delta_time;
            pitch = Clamp(pitch, -glm::half_pi<float>(), glm::half_pi<float>());

            rotation = Quaternion::fromAngleAxis(yaw, glm::vec3{0.0f, 1.0f, 0.0f});
            rotation = rotation * Quaternion::fromAngleAxis(pitch, glm::vec3{1.0f, 0.0f, 0.0f});

            forward = glm::normalize(rotation * glm::vec3{0.0f, 0.0f, -1.0f});
            right = glm::normalize(rotation * glm::vec3{1.0f, 0.0f, 0.0f});
            up = glm::normalize(glm::angleAxis(roll, forward) * glm::cross(right, forward));

            if(IsKeyDown(KEY_W)) { position += forward * keyboard_sensitivity * delta_time; }
            if(IsKeyDown(KEY_S)) { position -= forward * keyboard_sensitivity * delta_time; }
            if(IsKeyDown(KEY_A)) { position -= right * keyboard_sensitivity * delta_time; }
            if(IsKeyDown(KEY_D)) { position += right * keyboard_sensitivity * delta_time; }
            if(IsKeyDown(KEY_SPACE))      { position -= up * keyboard_sensitivity * delta_time; }
            if(IsKeyDown(KEY_LEFT_SHIFT)) { position += up * keyboard_sensitivity * delta_time; }
            if(IsKeyDown(KEY_Q)) { roll += delta_time * keyboard_sensitivity; }
            if(IsKeyDown(KEY_E)) { roll -= delta_time * keyboard_sensitivity; }
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

        float yaw{0.0f}, pitch{0.0f}, roll{0.0f};
        double fovy{0.0}, aspect{0.0}, near{0.01}, far{100.0};
        glm::mat4 perspective;
        Quaternion rotation{Quaternion::identity()};
        glm::vec3 position{0.0f};
        glm::vec3 forward{0.0f, 0.0f, -1.0f}, right{1.0f, 0.0f, 0.0f}, up{0.0f, 1.0f, 0.0f};
        float mouse_sensitivity{4.0f};
        float mouse_wheel_sensitivity{20.0f};
        float keyboard_sensitivity{6.0f};
    };
}

static void draw_cube(float window_width, float window_height, const gp::Camera& camera, const glm::mat4& transform) {
    for(auto i=0u; i<sizeof(cube_wire_frame) / sizeof(uint32_t); i+=2) {
        glm::vec4 a = transform * glm::vec4{cube_verts[cube_wire_frame[i]], 1.0f};
        glm::vec4 b = transform * glm::vec4{cube_verts[cube_wire_frame[i + 1]], 1.0f};

        if(glm::dot(glm::vec3{a} - camera.position, camera.forward) < 0.0f ||
           glm::dot(glm::vec3{b} - camera.position, camera.forward) < 0.0f) { continue; }
        
        a = camera.get_pv_mat() * a;
        b = camera.get_pv_mat() * b;
        a /= a.w;
        b /= b.w;

        if(glm::any(glm::greaterThan(glm::abs(a), glm::vec4{1.0f})) &&
           glm::any(glm::greaterThan(glm::abs(b), glm::vec4{1.0f}))) { continue; }
        
        a = a*0.5f + 0.5f;
        b = b*0.5f + 0.5f;
        a *= glm::vec4{window_width, window_height, 1.0f, 1.0f};
        b *= glm::vec4{window_width, window_height, 1.0f, 1.0f};

        DrawLine(a.x, a.y, b.x, b.y, PURPLE); 
    }
}

int main() {
    static constexpr auto window_width = 1280.0f;
    static constexpr auto window_height = 768.0f;
    static constexpr auto window_aspect = window_width / window_height;

    static glm::mat4 models[99];
    for(auto i=0u, idx=0u; i<10; ++i) {
        for(auto j=0u; j<10; ++j) {
            if(i == 5 && j == 5) { continue; }

            static constexpr auto offset = glm::vec3{-27.0f, 0.0f, -27.0f};
            const auto direction = glm::vec3{6.0f * i, 0.0f, 6.0f * j};

            models[idx] = glm::translate(glm::mat4{1.0f}, offset + direction);
            ++idx;
        }
    }

    InitWindow(window_width, window_height, "raylib [core] example - basic window");
    SetMousePosition(window_width * 0.5f, window_height * 0.5f);
    PollInputEvents();
    PollInputEvents();
    gp::Camera camera{PI/2.0, window_aspect};

    while (!WindowShouldClose())
    {
        camera.update();
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
#if 1
      
        for(auto i=0u; i<sizeof(models) / sizeof(models[0]); ++i) {
            draw_cube(window_width, window_height, camera, models[i]);
        }

        auto text = std::format("Camera position: [{:.2}, {:.2}, {:.2}]", camera.position.x, camera.position.y, camera.position.z);
        GuiWindowBox({0.0f, 0.0f, 200.0f, window_height}, "Controls");
        GuiLabel(Rectangle{0.0f, 0.0f, 200.0f, 100.0f}, text.c_str());
#endif
        EndDrawing();
    }

    CloseWindow();

    return 0;
}