#include <iostream>
#include <thread>
#include <chrono>
#include "capture_image_non_block.hpp"

int main() {
    std::cout << "Initializing V4L2 camera...\n";

    if (!init_camera("/dev/video0", 640, 480)) {
        std::cerr << "Failed to initialize camera.\n";
        return 1;
    }

    std::cout << "Capturing frames (press Ctrl+C to stop)...\n";

    while (true) {
        if (!capture_v4l2_frame("/dev/video0", "frame.jpg")) {
            std::cerr << "Capture timeout or error.\n";
        } else {
            std::cout << "Frame saved to 'frame.jpg'\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    close_camera();
    return 0;
}
