#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <wiringPi.h>
#include <chrono>
#include <climits>
#include <cmath>
#include <unistd.h>
#include "ads1115rpi.h"

#define MOSFET_WPI_PIN 6  // WiringPi Pin 6 = BCM GPIO 25

std::atomic<bool> keepRunning{true};
enum class SystemState { RUNNING, EMERGENCY };
std::atomic<SystemState> systemState{SystemState::RUNNING};

// Timing stats
long long min_exec_time = LLONG_MAX;
long long max_exec_time = 0;
long long total_exec_time = 0;
int exec_count = 0;
long long jitter = 0;

long long min_emergency_time = LLONG_MAX;
long long max_emergency_time = 0;
long long total_emergency_time = 0;
int emergency_count = 0;
long long emergency_jitter = 0;

class MQ7Callback : public ADS1115rpi::ADSCallbackInterface {
public:
    void hasADS1115Sample(float sample) override {
        auto start = std::chrono::steady_clock::now();

        if (sample > 1.1f && systemState != SystemState::EMERGENCY) {
            systemState = SystemState::EMERGENCY;
            std::cout << "ALERT: Gas level high! Emergency stop.\n";
        } else if (sample < 1.0f && systemState == SystemState::EMERGENCY) {
            systemState = SystemState::RUNNING;
            std::cout << "Gas level safe. Resuming.\n";
        }

        auto end = std::chrono::steady_clock::now();
        long long exec_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        min_emergency_time = std::min(min_emergency_time, exec_time);
        max_emergency_time = std::max(max_emergency_time, exec_time);
        total_emergency_time += exec_time;
        emergency_count++;

        long long avg = total_emergency_time / emergency_count;
        emergency_jitter = std::max(emergency_jitter, std::abs(exec_time - avg));
    }
};

void signalHandler(int signum) {
    std::cout << "\nSIGINT received. Stopping...\n";

    std::cout << "\n--- Control Service Timing Stats ---\n";
    std::cout << "Executions: " << exec_count << "\n";
    std::cout << "Min Time:   " << min_exec_time << " us\n";
    std::cout << "WCET Time:  " << max_exec_time << " us\n";
    std::cout << "Avg Time:   " << (total_exec_time / exec_count) << " us\n";
    std::cout << "Jitter:     " << jitter << " us\n";

    std::cout << "\n--- Gas Detection Timing Stats ---\n";
    std::cout << "Emergency Events: " << emergency_count << "\n";
    std::cout << "Min Time:         " << min_emergency_time << " us\n";
    std::cout << "WCET Time:        " << max_emergency_time << " us\n";
    std::cout << "Avg Time:         " << (total_emergency_time / emergency_count) << " us\n";
    std::cout << "Jitter:           " << emergency_jitter << " us\n";

    keepRunning = false;
}

void mosfet_control_loop() {
    while (keepRunning) {
        auto start = std::chrono::steady_clock::now();

        if (systemState == SystemState::EMERGENCY)
            digitalWrite(MOSFET_WPI_PIN, HIGH);
        else
            digitalWrite(MOSFET_WPI_PIN, LOW);

        auto end = std::chrono::steady_clock::now();
        long long exec_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        min_exec_time = std::min(min_exec_time, exec_time);
        max_exec_time = std::max(max_exec_time, exec_time);
        total_exec_time += exec_time;
        exec_count++;

        long long avg = total_exec_time / exec_count;
        jitter = std::max(jitter, std::abs(exec_time - avg));

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    signal(SIGINT, signalHandler);
    wiringPiSetup();

    pinMode(MOSFET_WPI_PIN, OUTPUT);
    digitalWrite(MOSFET_WPI_PIN, LOW);

    // Start sensor thread
    std::thread sensor_thread([] {
        MQ7Callback cb;
        ADS1115rpi reader;

        ADS1115settings settings;
        settings.channel = ADS1115settings::AIN0;
        settings.pgaGain = ADS1115settings::FSR2_048;
        settings.samplingRate = ADS1115settings::FS8HZ;

        reader.registerCallback(&cb);
        reader.start(settings);

        while (keepRunning)
            sleep(1);
    });

    // Start control loop
    mosfet_control_loop();

    sensor_thread.join();
    std::cout << "Shutdown complete.\n";
    return 0;
}
