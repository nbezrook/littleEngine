#ifndef LITTLEENGINE_APPLICATION_HPP
#define LITTLEENGINE_APPLICATION_HPP

#include "engine.h"

class Application {
    public:
        /**
         * @brief Construct an empty owner; engine startup waits until initialize is called.
         */
        Application() = default;

        /**
         * @brief Disable copying to prevent two owners from destroying the same engine resources.
         */
        Application(const Application&)= delete;
        /**
         * @brief Disable copy assignment to preserve exclusive engine ownership.
         */
        Application& operator=(const Application&)= delete;

        /**
         * @brief Initialize the owned engine once.
         * @param width Positive framebuffer width in pixels.
         * @param height Positive framebuffer height in pixels.
         * @param game_directory Asset root copied by the engine during initialization.
         * @return True after successful startup; false on startup failure or if already initialized.
         */
        bool initialize(int width, int height, const char *game_directory) {
            if (initialized_) return false;
        initialized_ = engine_init(&engine_, width, height, game_directory);
        return initialized_;
        }
         
        /**
         * @brief Run frames until the engine requests exit; do nothing before successful initialization.
         */
        void run() {
            if (!initialized_) return;

            while (engine_frame(&engine_)) {
            }
        }

        /**
         * @brief Shut down the owned engine if initialization succeeded.
         */
        ~Application() {
            if (initialized_) engine_shutdown(&engine_);
        }

    private:
        Engine engine_{};
        bool initialized_ = false;
    };

#endif
