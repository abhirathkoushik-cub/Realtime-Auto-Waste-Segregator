#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <pigpio.h>
#include <chrono>
#include <climits>
#include <cmath>

#include "Sequencer.hpp"

#define SERVO_GPIO 18  // BCM GPIO 18, physical pin 12
#define INPUT_GPIO 23  // BCM GPIO 23, physical pin 16

std::atomic<bool> keepRunning{true};

// Timing statistics
long long min_exec_time = LLONG_MAX;
long long max_exec_time = 0;
long long total_exec_time = 0;
long long jitter = 0;
int exec_count = 0;

void signalHandler(int signal)
{
    if (signal == SIGINT) {
        std::puts("\nCtrl+C received. Stopping services...");

        // Print timing stats BEFORE shutdown
        std::cout << "\n--- Timing Stats ---\n";
        std::cout << "Executions: " << exec_count << "\n";
        std::cout << "Min Time:   " << min_exec_time << " us\n";
        std::cout << "Max Time:   " << max_exec_time << " us\n";
        std::cout << "Avg Time:   " << (total_exec_time / exec_count) << " us\n";
        std::cout << "Jitter:     " << jitter << " us\n";

        keepRunning = false;
    }
}

void servo_task()
{
    static int lastState = -1;

    auto start = std::chrono::high_resolution_clock::now();

    int state = gpioRead(INPUT_GPIO);

    if (state != lastState) {
        auto timestamp = std::chrono::high_resolution_clock::now();
        long long time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            timestamp.time_since_epoch()).count();

        std::cout << "[DEBUG] GPIO " << INPUT_GPIO << " changed to " << state
                  << " at " << time_us << " us" << std::endl;

        if (state == 1) {
            gpioServo(SERVO_GPIO, 1500); // Rotate to 90°
            std::cout << "[servo] ? 90° at " << time_us << " us\n";
        } else {
            gpioServo(SERVO_GPIO, 500);  // Rotate to 0°
            std::cout << "[servo] ? 0° at " << time_us << " us\n";
        }

        lastState = state;
    }

    auto end = std::chrono::high_resolution_clock::now();
    long long exec_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Update timing stats
    if (exec_time < min_exec_time) min_exec_time = exec_time;
    if (exec_time > max_exec_time) max_exec_time = exec_time;
    total_exec_time += exec_time;
    exec_count++;

    long long avg = total_exec_time / exec_count;
    long long this_jitter = std::abs(exec_time - avg);
    if (this_jitter > jitter) jitter = this_jitter;
}

int main()
{
    std::signal(SIGINT, signalHandler);
    gpioCfgSetInternals(gpioCfgGetInternals() | PI_CFG_NOSIGHANDLER);

    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        return 1;
    }

    gpioSetMode(SERVO_GPIO, PI_OUTPUT);
    gpioSetMode(INPUT_GPIO, PI_INPUT);
    gpioSetPullUpDown(INPUT_GPIO, PI_PUD_DOWN);

    Sequencer sequencer;
    sequencer.addService(servo_task, 1, 99, 20, new Service_Statistic, 1);

    std::puts("Starting services...");
    sequencer.startServices();

    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // move stats printing here BEFORE pigpio terminates
    std::cout << "\n--- Timing Stats ---\n";
    std::cout << "Executions: " << exec_count << "\n";
    std::cout << "Min Time:   " << min_exec_time << " us\n";
    std::cout << "Max Time:   " << max_exec_time << " us\n";
    std::cout << "Avg Time:   " << (total_exec_time / exec_count) << " us\n";
    std::cout << "Jitter:     " << jitter << " us\n";

    sequencer.stopServices();
    gpioServo(SERVO_GPIO, 0);
    gpioTerminate();

    std::puts("Services are stopped\n");
    return 0;
}

