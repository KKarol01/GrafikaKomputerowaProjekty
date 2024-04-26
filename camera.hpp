#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

namespace gp {
    struct Quaternion {
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
    
    struct Camera {
        Camera(GLFWwindow* window, double fovy, double aspect);
        void update();
        glm::mat4 get_pv_mat() const;
        glm::mat4 get_v_mat() const;
        void change_perspective(double fovy, double aspect, double near, double far);

        GLFWwindow* window;
        float yaw{0.0f}, pitch{0.0f}, roll{0.0f};
        double fovy{0.0}, aspect{0.0}, near{0.01}, far{30.0};
        glm::mat4 perspective;
        Quaternion rotation{Quaternion::identity()};
        glm::vec3 position{0.0f};
        glm::vec3 forward{0.0f, 0.0f, -1.0f}, right{1.0f, 0.0f, 0.0f}, up{0.0f, 1.0f, 0.0f};
        float mouse_sensitivity{0.5f};
        float mouse_wheel_sensitivity{20.0f};
        float keyboard_sensitivity{5.0f};
        bool hide_cursor = true;
        glm::vec2 last_mouse_pos;
        double last_time;
    };
}

gp::Quaternion operator*(const gp::Quaternion& a, const gp::Quaternion& b);
glm::vec3 operator*(const gp::Quaternion& a, const glm::vec3& v);