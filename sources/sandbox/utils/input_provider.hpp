#pragma once
#include <nasral/inp/types.h>
#include <nasral/inp/provider.h>
#include <GLFW/glfw3.h>

namespace utils
{
    class GlfwInputProvider final : public nasral::inp::InputProvider
    {
    public:
        explicit GlfwInputProvider(GLFWwindow* window)
            : window_(window)
            , glfw_key_map_({})
            , glfw_mouse_button_map_({})
        {
            // Инициализация карты кнопок (GLFW <-> Nasral)
            init_key_map();
            init_mouse_button_map();

            // Устанавливаем user pointer на этот объект (для доступа в callbacks)
            glfwSetWindowUserPointer(window_, this);

            // Инициализируем позицию мыши
            double x_pos, y_pos;
            glfwGetCursorPos(window_, &x_pos, &y_pos);
            om_mouse_pos_changed(static_cast<float>(x_pos), static_cast<float>(y_pos));

            // Устанавливаем callbacks
            glfwSetKeyCallback(window_, key_callback);
            glfwSetCursorPosCallback(window_, mouse_pos_callback);
            glfwSetMouseButtonCallback(window_, mouse_button_callback);
        }

        ~GlfwInputProvider() override
        {
            glfwSetKeyCallback(window_, nullptr);
            glfwSetCursorPosCallback(window_, nullptr);
            glfwSetMouseButtonCallback(window_, nullptr);
        }

    private:
        GLFWwindow* window_ = nullptr;
        std::array<nasral::inp::KeyCode, GLFW_KEY_LAST + 1> glfw_key_map_;
        std::array<nasral::inp::MouseButton, GLFW_MOUSE_BUTTON_LAST + 1> glfw_mouse_button_map_;

        void init_key_map()
        {
            // Инициализируем массив значениями eNone по умолчанию (индексы 0 будут eNone)
            std::fill(glfw_key_map_.begin(), glfw_key_map_.end(), nasral::inp::KeyCode::eNone);

            // Алфавитно-цифровые клавиши
            glfw_key_map_[GLFW_KEY_A] = nasral::inp::KeyCode::eA;
            glfw_key_map_[GLFW_KEY_B] = nasral::inp::KeyCode::eB;
            glfw_key_map_[GLFW_KEY_C] = nasral::inp::KeyCode::eC;
            glfw_key_map_[GLFW_KEY_D] = nasral::inp::KeyCode::eD;
            glfw_key_map_[GLFW_KEY_E] = nasral::inp::KeyCode::eE;
            glfw_key_map_[GLFW_KEY_F] = nasral::inp::KeyCode::eF;
            glfw_key_map_[GLFW_KEY_G] = nasral::inp::KeyCode::eG;
            glfw_key_map_[GLFW_KEY_H] = nasral::inp::KeyCode::eH;
            glfw_key_map_[GLFW_KEY_I] = nasral::inp::KeyCode::eI;
            glfw_key_map_[GLFW_KEY_J] = nasral::inp::KeyCode::eJ;
            glfw_key_map_[GLFW_KEY_K] = nasral::inp::KeyCode::eK;
            glfw_key_map_[GLFW_KEY_L] = nasral::inp::KeyCode::eL;
            glfw_key_map_[GLFW_KEY_M] = nasral::inp::KeyCode::eM;
            glfw_key_map_[GLFW_KEY_N] = nasral::inp::KeyCode::eN;
            glfw_key_map_[GLFW_KEY_O] = nasral::inp::KeyCode::eO;
            glfw_key_map_[GLFW_KEY_P] = nasral::inp::KeyCode::eP;
            glfw_key_map_[GLFW_KEY_Q] = nasral::inp::KeyCode::eQ;
            glfw_key_map_[GLFW_KEY_R] = nasral::inp::KeyCode::eR;
            glfw_key_map_[GLFW_KEY_S] = nasral::inp::KeyCode::eS;
            glfw_key_map_[GLFW_KEY_T] = nasral::inp::KeyCode::eT;
            glfw_key_map_[GLFW_KEY_U] = nasral::inp::KeyCode::eU;
            glfw_key_map_[GLFW_KEY_V] = nasral::inp::KeyCode::eV;
            glfw_key_map_[GLFW_KEY_W] = nasral::inp::KeyCode::eW;
            glfw_key_map_[GLFW_KEY_X] = nasral::inp::KeyCode::eX;
            glfw_key_map_[GLFW_KEY_Y] = nasral::inp::KeyCode::eY;
            glfw_key_map_[GLFW_KEY_Z] = nasral::inp::KeyCode::eZ;
            glfw_key_map_[GLFW_KEY_0] = nasral::inp::KeyCode::e0;
            glfw_key_map_[GLFW_KEY_1] = nasral::inp::KeyCode::e1;
            glfw_key_map_[GLFW_KEY_2] = nasral::inp::KeyCode::e2;
            glfw_key_map_[GLFW_KEY_3] = nasral::inp::KeyCode::e3;
            glfw_key_map_[GLFW_KEY_4] = nasral::inp::KeyCode::e4;
            glfw_key_map_[GLFW_KEY_5] = nasral::inp::KeyCode::e5;
            glfw_key_map_[GLFW_KEY_6] = nasral::inp::KeyCode::e6;
            glfw_key_map_[GLFW_KEY_7] = nasral::inp::KeyCode::e7;
            glfw_key_map_[GLFW_KEY_8] = nasral::inp::KeyCode::e8;
            glfw_key_map_[GLFW_KEY_9] = nasral::inp::KeyCode::e9;

            // Функциональные клавиши
            glfw_key_map_[GLFW_KEY_F1] = nasral::inp::KeyCode::eF1;
            glfw_key_map_[GLFW_KEY_F2] = nasral::inp::KeyCode::eF2;
            glfw_key_map_[GLFW_KEY_F3] = nasral::inp::KeyCode::eF3;
            glfw_key_map_[GLFW_KEY_F4] = nasral::inp::KeyCode::eF4;
            glfw_key_map_[GLFW_KEY_F5] = nasral::inp::KeyCode::eF5;
            glfw_key_map_[GLFW_KEY_F6] = nasral::inp::KeyCode::eF6;
            glfw_key_map_[GLFW_KEY_F7] = nasral::inp::KeyCode::eF7;
            glfw_key_map_[GLFW_KEY_F8] = nasral::inp::KeyCode::eF8;
            glfw_key_map_[GLFW_KEY_F9] = nasral::inp::KeyCode::eF9;
            glfw_key_map_[GLFW_KEY_F10] = nasral::inp::KeyCode::eF10;
            glfw_key_map_[GLFW_KEY_F11] = nasral::inp::KeyCode::eF11;
            glfw_key_map_[GLFW_KEY_F12] = nasral::inp::KeyCode::eF12;

            // Управляющие клавиши
            glfw_key_map_[GLFW_KEY_ESCAPE] = nasral::inp::KeyCode::eEscape;
            glfw_key_map_[GLFW_KEY_ENTER] = nasral::inp::KeyCode::eEnter;
            glfw_key_map_[GLFW_KEY_TAB] = nasral::inp::KeyCode::eTab;
            glfw_key_map_[GLFW_KEY_BACKSPACE] = nasral::inp::KeyCode::eBackspace;
            glfw_key_map_[GLFW_KEY_INSERT] = nasral::inp::KeyCode::eInsert;
            glfw_key_map_[GLFW_KEY_DELETE] = nasral::inp::KeyCode::eDelete;
            glfw_key_map_[GLFW_KEY_RIGHT] = nasral::inp::KeyCode::eRight;
            glfw_key_map_[GLFW_KEY_LEFT] = nasral::inp::KeyCode::eLeft;
            glfw_key_map_[GLFW_KEY_DOWN] = nasral::inp::KeyCode::eDown;
            glfw_key_map_[GLFW_KEY_UP] = nasral::inp::KeyCode::eUp;
            glfw_key_map_[GLFW_KEY_PAGE_UP] = nasral::inp::KeyCode::ePageUp;
            glfw_key_map_[GLFW_KEY_PAGE_DOWN] = nasral::inp::KeyCode::ePageDown;
            glfw_key_map_[GLFW_KEY_HOME] = nasral::inp::KeyCode::eHome;
            glfw_key_map_[GLFW_KEY_END] = nasral::inp::KeyCode::eEnd;
            glfw_key_map_[GLFW_KEY_CAPS_LOCK] = nasral::inp::KeyCode::eCapsLock;
            glfw_key_map_[GLFW_KEY_SCROLL_LOCK] = nasral::inp::KeyCode::eScrollLock;
            glfw_key_map_[GLFW_KEY_NUM_LOCK] = nasral::inp::KeyCode::eNumLock;
            glfw_key_map_[GLFW_KEY_PRINT_SCREEN] = nasral::inp::KeyCode::ePrintScreen;
            glfw_key_map_[GLFW_KEY_PAUSE] = nasral::inp::KeyCode::ePause;

            // Модификаторы
            glfw_key_map_[GLFW_KEY_LEFT_SHIFT] = nasral::inp::KeyCode::eLeftShift;
            glfw_key_map_[GLFW_KEY_RIGHT_SHIFT] = nasral::inp::KeyCode::eRightShift;
            glfw_key_map_[GLFW_KEY_LEFT_CONTROL] = nasral::inp::KeyCode::eLeftControl;
            glfw_key_map_[GLFW_KEY_RIGHT_CONTROL] = nasral::inp::KeyCode::eRightControl;
            glfw_key_map_[GLFW_KEY_LEFT_ALT] = nasral::inp::KeyCode::eLeftAlt;
            glfw_key_map_[GLFW_KEY_RIGHT_ALT] = nasral::inp::KeyCode::eRightAlt;
            glfw_key_map_[GLFW_KEY_LEFT_SUPER] = nasral::inp::KeyCode::eLeftSuper;
            glfw_key_map_[GLFW_KEY_RIGHT_SUPER] = nasral::inp::KeyCode::eRightSuper;
            glfw_key_map_[GLFW_KEY_MENU] = nasral::inp::KeyCode::eMenu;

            // Спецсимволы
            glfw_key_map_[GLFW_KEY_SPACE] = nasral::inp::KeyCode::eSpace;
            glfw_key_map_[GLFW_KEY_APOSTROPHE] = nasral::inp::KeyCode::eApostrophe;
            glfw_key_map_[GLFW_KEY_COMMA] = nasral::inp::KeyCode::eComma;
            glfw_key_map_[GLFW_KEY_MINUS] = nasral::inp::KeyCode::eMinus;
            glfw_key_map_[GLFW_KEY_PERIOD] = nasral::inp::KeyCode::ePeriod;
            glfw_key_map_[GLFW_KEY_SLASH] = nasral::inp::KeyCode::eSlash;
            glfw_key_map_[GLFW_KEY_SEMICOLON] = nasral::inp::KeyCode::eSemicolon;
            glfw_key_map_[GLFW_KEY_EQUAL] = nasral::inp::KeyCode::eEqual;
            glfw_key_map_[GLFW_KEY_LEFT_BRACKET] = nasral::inp::KeyCode::eLeftBracket;
            glfw_key_map_[GLFW_KEY_BACKSLASH] = nasral::inp::KeyCode::eBackslash;
            glfw_key_map_[GLFW_KEY_RIGHT_BRACKET] = nasral::inp::KeyCode::eRightBracket;
            glfw_key_map_[GLFW_KEY_GRAVE_ACCENT] = nasral::inp::KeyCode::eGraveAccent;

            // Клавиши цифровой клавиатуры (Numpad)
            glfw_key_map_[GLFW_KEY_KP_0] = nasral::inp::KeyCode::eKp0;
            glfw_key_map_[GLFW_KEY_KP_1] = nasral::inp::KeyCode::eKp1;
            glfw_key_map_[GLFW_KEY_KP_2] = nasral::inp::KeyCode::eKp2;
            glfw_key_map_[GLFW_KEY_KP_3] = nasral::inp::KeyCode::eKp3;
            glfw_key_map_[GLFW_KEY_KP_4] = nasral::inp::KeyCode::eKp4;
            glfw_key_map_[GLFW_KEY_KP_5] = nasral::inp::KeyCode::eKp5;
            glfw_key_map_[GLFW_KEY_KP_6] = nasral::inp::KeyCode::eKp6;
            glfw_key_map_[GLFW_KEY_KP_7] = nasral::inp::KeyCode::eKp7;
            glfw_key_map_[GLFW_KEY_KP_8] = nasral::inp::KeyCode::eKp8;
            glfw_key_map_[GLFW_KEY_KP_9] = nasral::inp::KeyCode::eKp9;
            glfw_key_map_[GLFW_KEY_KP_DECIMAL] = nasral::inp::KeyCode::eKpDecimal;
            glfw_key_map_[GLFW_KEY_KP_DIVIDE] = nasral::inp::KeyCode::eKpDivide;
            glfw_key_map_[GLFW_KEY_KP_MULTIPLY] = nasral::inp::KeyCode::eKpMultiply;
            glfw_key_map_[GLFW_KEY_KP_SUBTRACT] = nasral::inp::KeyCode::eKpSubtract;
            glfw_key_map_[GLFW_KEY_KP_ADD] = nasral::inp::KeyCode::eKpAdd;
            glfw_key_map_[GLFW_KEY_KP_ENTER] = nasral::inp::KeyCode::eKpEnter;
            glfw_key_map_[GLFW_KEY_KP_EQUAL] = nasral::inp::KeyCode::eKpEqual;
        }

        void init_mouse_button_map()
        {
            std::fill(glfw_mouse_button_map_.begin(), glfw_mouse_button_map_.end(), nasral::inp::MouseButton::eNone);
            glfw_mouse_button_map_[GLFW_MOUSE_BUTTON_LEFT] = nasral::inp::MouseButton::eLeft;
            glfw_mouse_button_map_[GLFW_MOUSE_BUTTON_RIGHT] = nasral::inp::MouseButton::eRight;
            glfw_mouse_button_map_[GLFW_MOUSE_BUTTON_MIDDLE] = nasral::inp::MouseButton::eMiddle;
        }

        static void key_callback(GLFWwindow* window
            , const int key
            , [[maybe_unused]] const int scancode
            , const int action
            , [[maybe_unused]] const int mods)
        {
            auto* provider = static_cast<GlfwInputProvider*>(glfwGetWindowUserPointer(window));
            if (provider && key >= 0 && key < GLFW_KEY_LAST + 1){
                const nasral::inp::KeyCode mapped_key = provider->glfw_key_map_[key];
                if (mapped_key != nasral::inp::KeyCode::eNone) {
                    provider->on_key_state_changed(mapped_key, action == GLFW_PRESS || action == GLFW_REPEAT);
                }
            }
        }

        static void mouse_pos_callback(GLFWwindow* window
            , const double x_pos
            , const double y_pos)
        {
            if (auto* provider = static_cast<GlfwInputProvider*>(glfwGetWindowUserPointer(window))) {
                provider->om_mouse_pos_changed(
                    static_cast<float>(x_pos),
                    static_cast<float>(y_pos));
            }
        }

        static void mouse_button_callback(GLFWwindow* window
            , const int button
            , const int action
            , [[maybe_unused]] const int mods)
        {
            auto* provider = static_cast<GlfwInputProvider*>(glfwGetWindowUserPointer(window));
            if (provider && button >= 0 && button < GLFW_MOUSE_BUTTON_LAST + 1) {
                const nasral::inp::MouseButton mapped_button = provider->glfw_mouse_button_map_[button];
                if (mapped_button != nasral::inp::MouseButton::eNone) {
                    provider->on_mouse_btn_state_changed(mapped_button, action == GLFW_PRESS);
                }
            }
        }
    };
}