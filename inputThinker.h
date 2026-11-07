#ifndef LITTLEENGINE_INPUTTHINKER_H
#define LITTLEENGINE_INPUTTHINKER_H

#include "include/SDL3/SDL.h"

typedef struct {
    float forward;
    float right;
} InputMovement;

typedef struct {
    float yaw;
    float pitch;
} InputLook;

typedef struct {
    SDL_Window *window; 

    bool forward_down;
    bool backward_down;
    bool left_down;
    bool right_down;

    float pending_yaw;
    float pending_pitch;
    float mouse_sensitivity;
} InputThinker;



#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset input state and associate it with a borrowed SDL window.
 * @param input Destination state; NULL is rejected.
 * @param window Existing game window; NULL is rejected. Must outlive input use.
 * @return True on success; false for a null argument, leaving state unchanged.
 * @note Sets sensitivity to 0.0025 radians per mouse-motion unit. Does not capture the mouse.
 */
bool input_init(InputThinker *input, SDL_Window *window);
/**
 * @brief Update held WASD state from one event routed by the main event loop.
 * @param input Initialized input state; NULL is ignored.
 * @param event Borrowed event; NULL and events for other windows are ignored.
 * @note Focus loss clears held keys and pending look. Does not poll SDL's event queue.
 * Mouse capture and mouse-motion accumulation are not implemented yet.
 * Call on the main thread while the associated window exists.
 */
void input_event(InputThinker *input, const SDL_Event *event);
/**
 * @brief Read movement intent without consuming held-key state.
 * @param input Input state, or NULL for zero movement.
 * @return Forward = W - S and right = D - A; each component is in [-1, 1].
 * @note The diagonal is not normalized here; the movement controller handles that.
 */
InputMovement input_get_movement(const InputThinker *input);
/**
 * @brief Return accumulated angular displacement and reset it to zero.
 * @param input Input state, or NULL for zero displacement.
 * @return Pending yaw and pitch in radians, to apply once per consumption.
 * @note Mouse events do not populate this state yet. Reading does not clear held keys.
 */
InputLook input_take_look(InputThinker *input);
/**
 * @brief Reset input state; accepts NULL and does not free the structure or window.
 * @param input State to reset.
 * @note No capture resource is currently acquired. Add capture release when capture is implemented.
 */
void input_shutdown(InputThinker *input);

#ifdef __cplusplus
}
#endif

#endif