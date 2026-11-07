#ifndef LITTLEENGINE_APPLICATION_HPP
#define LITTLEENGINE_APPLICATION_HPP

#include "engine.h"

class Application {
    public:
        Application() = default;

        Application(const Application&)= delete;
        Application& operator=(const Application&)= delete;

        bool initialize(int width, int height) {
            if (initialized_) return false;
        initialized_ = engine_init(&engine_, width, height);
        return initialized_;
        }
         
        void run() {
            if (!initialized_) return;

            while (engine_frame(&engine_)) {
            }
        }

        ~Application() {
            if (initialized_) engine_shutdown(&engine_);
        }

    private:
        Engine engine_{};
        bool initialized_ = false;
    };

#endif