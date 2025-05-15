#include <iostream>
#include <csignal>
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include "ads1115rpi.h"
#include "servo.hpp"
#include "Sequencer.hpp"
#include <chrono>
#include <atomic>
#include <condition_variable>
#include "persistent_v4l2_camera.hpp"

#define MOSFET_WPI_PIN 6
#define TRIG_PIN 4
#define ECHO_PIN 5

std::atomic<bool> keepRunning{true};
std::atomic<bool> frame_ready(false);
std::atomic<bool> processing_in_progress(false);
std::atomic<bool> stop_threads(false);
std::mutex frame_mutex, mtx;
std::condition_variable cv_capture;
std::string saved_image_path = "capture.jpg";

enum class SystemState { RUNNING, EMERGENCY };
std::atomic<SystemState> systemState{SystemState::RUNNING};

void signalHandler(int signum) {
    std::cout << "\nSIGINT received. Stopping...\n";
    keepRunning = false;
    stop_threads = true;
}

class MQ7Callback : public ADS1115rpi::ADSCallbackInterface {
public:
    void hasADS1115Sample(float sample) override {
        if (sample > 1.9f && systemState != SystemState::EMERGENCY) {
            systemState = SystemState::EMERGENCY;
            std::cout << "ALERT: Gas level high! Emergency stop.\n";
        } else if (sample < 1.7f && systemState == SystemState::EMERGENCY) {
            systemState = SystemState::RUNNING;
            std::cout << "Gas level safe. Resuming.\n";
        }
    }
};

void gas_service() {
    static MQ7Callback cb;
    static ADS1115rpi reader;
    static bool initialized = false;

    if (!initialized) {
        ADS1115settings settings;
        settings.channel = ADS1115settings::AIN0;
        settings.pgaGain = ADS1115settings::FSR2_048;
        settings.samplingRate = ADS1115settings::FS860HZ; // Changing sampling rate from 8 samples/sec to 860 samples/sec
        reader.registerCallback(&cb);
        reader.start(settings);
        initialized = true;
    }

    if (systemState == SystemState::EMERGENCY)
        digitalWrite(MOSFET_WPI_PIN, HIGH);
    else
        digitalWrite(MOSFET_WPI_PIN, LOW);
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


void capture_frames(PersistentV4L2Camera& camera) {
    // auto start = std::chrono::steady_clock::now();
    
    if (processing_in_progress) return;
    float distance = measure_distance();
    std::cout << "Measured distance: " << distance << " cm\n";
    if (distance < 20.0) {
        if (camera.captureToFile(saved_image_path)) {
            frame_ready = true;
            processing_in_progress = true;
            std::cout << "Captured " << saved_image_path << "\n";
        } else {
            std::cerr << "Failed to capture frame\n";
        }
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // processing_in_progress = false;
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

void inference_service() {
    if (!frame_ready) return;

    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame_ready = false;
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::string output = run_python_script(saved_image_path);
    if (!output.empty()) {
        std::string detected_class = extract_json_field(output, "class");
        std::string confidence = extract_json_field(output, "confidence");
        std::string inference_time = extract_json_field(output, "inference_time_ms");
        if (detected_class == "biodegradable") sweep_servo_1();
        else if (detected_class == "nonbiodegradable") sweep_servo_2();
        else std::cout << "Unknown detection result!\n";

        std::cout << "Detected Class   : " << detected_class << "\n";
        std::cout << "Confidence       : " << confidence << "\n";
        std::cout << "Inference Time   : " << inference_time << " ms\n";

        processing_in_progress = false;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Time taken for Inference: " << duration_ms << " ms\n";
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

    PersistentV4L2Camera camera("/dev/video0");

    Sequencer seq;
    seq.addService("Gas Monitor", gas_service, 1, 99, 100);
    seq.addService("Camera + Distance", [&camera]() { capture_frames(camera); }, 1, 98, 200);
    seq.addService("Inference", inference_service, 2, 99, 300);

    seq.startServices();
    std::cout << "Press Ctrl+C to stop...\n";

    while (keepRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    seq.stopServices();
    std::cout << "System shutdown complete.\n";
    return 0;
}
