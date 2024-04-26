#include "camera.hpp"

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
    Camera::Camera(GLFWwindow* window, double fovy, double aspect) : window(window), fovy(fovy), aspect(aspect), last_mouse_pos(mouse_position), last_time(glfwGetTime()) { 
        change_perspective(fovy, aspect, near, far); 

        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        mouse_position = {xpos, ypos};
    }

    void Camera::update() {
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

        scrollY = 0.0f;
    }

    glm::mat4 Camera::get_pv_mat() const {
        return perspective * get_v_mat();
    }

    glm::mat4 Camera::get_v_mat() const {
        return glm::lookAt(position, position + forward, up);
    }

    void Camera::change_perspective(double fovy, double aspect, double near, double far) {
        const auto S = 1.0f / glm::tan(fovy * 0.5f);
        const auto Z = - far / (far - near);
        perspective = glm::mat4{
            glm::vec4{S, 0,          0,       0},
            glm::vec4{0, S * aspect, 0,       0},
            glm::vec4{0, 0,          Z,      -1},
            glm::vec4{0, 0,          Z*near,  0}
        };
    }
}

gp::Quaternion operator*(const gp::Quaternion& a, const gp::Quaternion& b) {
    const auto vec_a = glm::vec3{a.x, a.y, a.z};
    const auto vec_b = glm::vec3{b.x, b.y, b.z};

    return {
        a.a * b.a - glm::dot(vec_a, vec_b),
        a.a*vec_b + b.a*vec_a + glm::cross(vec_a, vec_b)
    };
}

glm::vec3 operator*(const gp::Quaternion& a, const glm::vec3& v) {
    const auto res = a * gp::Quaternion{0.0f, v} * a.conjugate();
    return glm::vec3{res.x, res.y, res.z};
}