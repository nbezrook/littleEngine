#include "inputThinker.h"

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
    input->forward_down = false;
    input->backward_down = false;
    input->left_down = false;
    input->right_down = false;
    input->pending_yaw = 0.0f;
    input->pending_pitch = 0.0f;
    return;
    }

    if (event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP) return;

    bool down = event->key.down;

    switch (event->key.scancode) {
        case SDL_SCANCODE_W: input->forward_down = down; break;
        case SDL_SCANCODE_S: input->backward_down = down; break;
        case SDL_SCANCODE_A: input->left_down = down; break;
        case SDL_SCANCODE_D: input->right_down = down; break;
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

void input_shutdown(InputThinker *input){
    if (input)
        *input = InputThinker{};
}
