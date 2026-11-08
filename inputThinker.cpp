#include "inputThinker.h"

/**
 * @brief Enable or release relative mouse mode on the main thread.
 * @param input Initialized input borrowing a live game window.
 * @param capture True to capture; false to release.
 */
static void input_set_capture(InputThinker *input, bool capture)
{
    if (!SDL_SetWindowRelativeMouseMode(input->window, capture)) {
        SDL_Log("Mouse capture failed: %s", SDL_GetError());
        return;
    }
    input->pending_yaw = 0.0f;
    input->pending_pitch = 0.0f;
}

bool input_init(InputThinker *input, SDL_Window *window) {
    if (!input || !window) return false;

    *input = InputThinker{};
    input->window = window;
    input->mouse_sensitivity = 0.0025f;

    return true;
}

void input_event(InputThinker *input, const SDL_Event *event){
    if (!input || !input->window || !event) return;

    if (SDL_GetWindowFromEvent(event) != input->window) return;

    // remove control when window focus is lost
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        input_set_capture(input, false);

        input->forward_down = false;
        input->backward_down = false;
        input->left_down = false;
        input->right_down = false;
        input->pending_yaw = 0.0f;
        input->pending_pitch = 0.0f;
        input->jump_pressed = false;
        return;
    }

    /* Click the game window to capture the mouse. */
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        input_set_capture(input, true);
        return;
    }

    /* Escape releases the cursor for the console and desktop. */
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_ESCAPE && !event->key.repeat) {
        input_set_capture(input, false);
        return;
    }

    /* Accumulate angular displacement while captured. */
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        if (SDL_GetWindowRelativeMouseMode(input->window)) {
            input->pending_yaw += event->motion.xrel * input->mouse_sensitivity;
            input->pending_pitch -= event->motion.yrel * input->mouse_sensitivity;
        }
        return;
    }


    if (event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP) return;

    bool down = event->key.down;

    switch (event->key.scancode) {
        case SDL_SCANCODE_W: input->forward_down = down; break;
        case SDL_SCANCODE_S: input->backward_down = down; break;
        case SDL_SCANCODE_A: input->left_down = down; break;
        case SDL_SCANCODE_D: input->right_down = down; break;
        case SDL_SCANCODE_SPACE:
            if (down && !event->key.repeat)
                input->jump_pressed = true;
            break;
        default: break;
    }
}

InputMovement input_get_movement(const InputThinker *input) {
    if (!input) return InputMovement{};
    return InputMovement{float(input->forward_down) - float(input->backward_down), float(input->right_down) - float(input->left_down)};
}

InputLook input_take_look(InputThinker *input) {
    if (!input) return InputLook{};

    InputLook look{input->pending_yaw, input->pending_pitch};

    input->pending_yaw = 0.0f;
    input->pending_pitch = 0.0f;

    return look;
}

void input_shutdown(InputThinker *input)
{
    if (!input) return;
    if (input->window) input_set_capture(input, false);
    *input = InputThinker{};
}

bool input_take_jump(InputThinker *input)
{
    if (!input) return false;

    bool pressed = input->jump_pressed;
    input->jump_pressed = false;
    return pressed;
}