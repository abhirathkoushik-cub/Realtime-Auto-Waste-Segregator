#include <iostream>
#include <csignal>
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <sched.h>
#include "ads1115rpi.h"
#include "servo.hpp"
#include "Sequencer.hpp"

#define MOSFET_WPI_PIN 6
#define TRIG_PIN 4
#define ECHO_PIN 5

std::atomic<bool> keepRunning{true};
std::atomic<bool> stop_threads(false);
std::atomic<bool> frame_ready(false);
std::atomic<bool> processing_in_progress(false);
std::mutex frame_mutex;
std::string saved_image_path = "capture.jpg";

enum class SystemState { RUNNING, EMERGENCY };
std::atomic<SystemState> systemState{SystemState::RUNNING};

void signalHandler(int signum) {
    std::cout << "\nSIGINT received. Stopping...\n";
    keepRunning = false;
}

// Set thread affinity to a specific core
void set_thread_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "Failed to set thread affinity to core " << core_id << "\n";
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

void gas_service() {
    set_thread_affinity(1);  // Run this thread on Core 1

    static MQ7Callback cb;
    static ADS1115rpi reader;
    static bool initialized = false;

    if (!initialized) {
        ADS1115settings settings;
        settings.channel = ADS1115settings::AIN0;
        settings.pgaGain = ADS1115settings::FSR2_048;
        settings.samplingRate = ADS1115settings::FS8HZ;
        reader.registerCallback(&cb);
        reader.start(settings);
        initialized = true;
    }

    if (systemState == SystemState::EMERGENCY)
        digitalWrite(MOSFET_WPI_PIN, HIGH);  // MOSFET OFF
    else
        digitalWrite(MOSFET_WPI_PIN, LOW);   // MOSFET ON
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

void capture_service() {
    set_thread_affinity(1);  // Run this thread on Core 1

    if (processing_in_progress) return;

    float distance = measure_distance();
    std::cout << "Measured distance: " << distance << " cm\n";

    if (distance < 30.0) {
        cv::VideoCapture cap(0);
        if (!cap.isOpened()) {
            std::cerr << "Error: Could not open camera!\n";
            return;
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
    set_thread_affinity(2);  // Run this thread on Core 2

    if (!frame_ready) return;

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

    Sequencer seq;
    seq.addService("Gas Monitor", gas_service, 0, 50, 100);
    seq.addService("Camera + Distance", capture_service, 1, 51, 200);
    seq.addService("Inference", inference_service, 2, 52, 300);

    seq.startServices();
    std::cout << "Press Ctrl+C to stop...\n";

    while (keepRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    seq.stopServices();
    std::cout << "System shutdown complete.\n";
    return 0;
}
