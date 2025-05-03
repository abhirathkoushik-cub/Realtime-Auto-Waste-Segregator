#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <csignal>
#include <climits>
#include <cmath>
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include "ads1115rpi.h"
#include "servo.hpp"

#define MOSFET_WPI_PIN 6
#define TRIG_PIN 4
#define ECHO_PIN 5

std::atomic<bool> keepRunning{true};
std::condition_variable cv_gas, cv_capture, cv_inference;
std::mutex mtx;

std::atomic<bool> stop_threads(false);
std::atomic<bool> frame_ready(false);
std::atomic<bool> processing_in_progress(false);
std::mutex frame_mutex;
std::string saved_image_path = "capture.jpg";

enum class SystemState { RUNNING, EMERGENCY };
std::atomic<SystemState> systemState{SystemState::RUNNING};

long long min_exec_time[3] = {LLONG_MAX, LLONG_MAX, LLONG_MAX};
long long max_exec_time[3] = {0, 0, 0};
long long total_exec_time[3] = {0, 0, 0};
int exec_count[3] = {0, 0, 0};
long long jitter[3] = {0, 0, 0};

void update_timing(int id, long long exec_time) {
    min_exec_time[id] = std::min(min_exec_time[id], exec_time);
    max_exec_time[id] = std::max(max_exec_time[id], exec_time);
    total_exec_time[id] += exec_time;
    exec_count[id]++;
    long long avg = total_exec_time[id] / exec_count[id];
    jitter[id] = std::max(jitter[id], std::abs(exec_time - avg));
}

void signalHandler(int signum) {
    std::cout << "\nSIGINT received. Stopping...\n";
    keepRunning = false;
    stop_threads = true;
    cv_gas.notify_all();
    cv_capture.notify_all();
    cv_inference.notify_all();

    for (int i = 0; i < 3; ++i) {
        std::cout << "\n--- Thread " << i + 1 << " Timing Stats ---\n";
        std::cout << "Executions: " << exec_count[i] << "\n";
        std::cout << "Min Time:   " << min_exec_time[i] << " us\n";
        std::cout << "WCET Time:  " << max_exec_time[i] << " us\n";
        std::cout << "Avg Time:   " << (exec_count[i] ? total_exec_time[i] / exec_count[i] : 0) << " us\n";
        std::cout << "Jitter:     " << jitter[i] << " us\n";
    }
}

class MQ7Callback : public ADS1115rpi::ADSCallbackInterface {
public:
    void hasADS1115Sample(float sample) override {
        if (sample > 1.1f && systemState != SystemState::EMERGENCY) {
            systemState = SystemState::EMERGENCY;
            std::cout << "ALERT: Gas level high! Emergency stop.\n";
        } else if (sample < 1.0f && systemState == SystemState::EMERGENCY) {
            systemState = SystemState::RUNNING;
            std::cout << "Gas level safe. Resuming.\n";
        }
    }
};

void gas_and_mosfet_thread() {
    MQ7Callback cb;
    ADS1115rpi reader;
    ADS1115settings settings;
    settings.channel = ADS1115settings::AIN0;
    settings.pgaGain = ADS1115settings::FSR2_048;
    settings.samplingRate = ADS1115settings::FS8HZ;
    reader.registerCallback(&cb);
    reader.start(settings);

    while (keepRunning) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_gas.wait(lock);

        auto start = std::chrono::steady_clock::now();

        if (!keepRunning) break;

        if (systemState == SystemState::EMERGENCY)
            digitalWrite(MOSFET_WPI_PIN, HIGH);
        else
            digitalWrite(MOSFET_WPI_PIN, LOW);

        auto end = std::chrono::steady_clock::now();
        update_timing(0, std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
}

float measure_distance() {
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    while (digitalRead(ECHO_PIN) == LOW);
    long start_time = micros();
    while (digitalRead(ECHO_PIN) == HIGH);
    long end_time = micros();
    return (end_time - start_time) * 0.0343 / 2.0;
}

void capture_frames() {
    while (!stop_threads) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_capture.wait(lock);

        auto start = std::chrono::steady_clock::now();

        if (processing_in_progress) continue;
        float distance = measure_distance();
        std::cout << "Measured distance: " << distance << " cm\n";

        if (distance < 20.0) {
            cv::VideoCapture cap(0);
            if (!cap.isOpened()) {
                std::cerr << "Error: Could not open camera!\n";
                continue;
            }
            usleep(500000);
            cv::Mat frame;
            cap >> frame;
            if (!frame.empty()) {
                std::lock_guard<std::mutex> lock(frame_mutex);
                cv::imwrite(saved_image_path, frame);
                frame_ready = true;
                processing_in_progress = true;
                std::cout << "Object detected! Frame captured and saved!\n";
            }
            cap.release();
        }

        auto end = std::chrono::steady_clock::now();
        update_timing(1, std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
}

std::string run_python_script(const std::string& image_file) {
    std::string cmd = "/home/abhirathkoushik/RTES_files/RTES_final_project/myenv/bin/python3 predict_tflite.py " + image_file;
    std::string result;
    char buffer[256];
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

std::string extract_json_field(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    size_t start = json.find_first_not_of(" \":", pos);
    size_t end = json.find_first_of(",}", start);
    std::string field = json.substr(start, end - start);
    field.erase(remove(field.begin(), field.end(), '"'), field.end());
    return field;
}

void run_inference() {
    while (!stop_threads) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_inference.wait(lock);

        auto start = std::chrono::steady_clock::now();

        if (!frame_ready) continue;

        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            frame_ready = false;
        }

        std::string output = run_python_script(saved_image_path);
        if (!output.empty()) {
            std::string detected_class = extract_json_field(output, "class");
            if (detected_class == "biodegradable") sweep_servo_1();
            else if (detected_class == "nonbiodegradable") sweep_servo_2();
            else std::cout << "Unknown detection result!\n";
            processing_in_progress = false;
        }

        auto end = std::chrono::steady_clock::now();
        update_timing(2, std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
}

int main() {
    signal(SIGINT, signalHandler);
    wiringPiSetup();
    pinMode(MOSFET_WPI_PIN, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(MOSFET_WPI_PIN, LOW);
    digitalWrite(TRIG_PIN, LOW);

    init_servos();
    set_servo2_initial();
    set_servo1_initial();

    std::thread t1(gas_and_mosfet_thread);
    std::thread t2(capture_frames);
    std::thread t3(run_inference);

    int count = 0;
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cv_gas.notify_one();
        if (count % 5 == 0) cv_capture.notify_one();
        if (count % 10 == 0) cv_inference.notify_one();
        count++;
    }

    t1.join();
    t2.join();
    t3.join();
    std::cout << "System shutdown complete.\n";
    return 0;
}
