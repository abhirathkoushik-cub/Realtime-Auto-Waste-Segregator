


#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <opencv2/opencv.hpp>
#include <cstdio>  
#include <memory>
#include <wiringPi.h>
#include <unistd.h> 
#include "servo.hpp"    

std::atomic<bool> stop_threads(false);
std::atomic<bool> frame_ready(false);
std::atomic<bool> processing_in_progress(false);
std::mutex frame_mutex;
std::string saved_image_path = "capture.jpg";

const int TRIG_PIN = 4;   // WiringPi Pin 4 = GPIO23 (physical pin 16)
const int ECHO_PIN = 5;   // WiringPi Pin 5 = GPIO24 (physical pin 18)

void setup_gpio()
{
    wiringPiSetup();
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);
}

float measure_distance() {
    // Send a 10us pulse to trigger
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Wait for echo start
    while (digitalRead(ECHO_PIN) == LOW);

    long start_time = micros();

    // Wait for echo end
    while (digitalRead(ECHO_PIN) == HIGH);

    long end_time = micros();

    // Calculate distance
    float distance = (end_time - start_time) * 0.0343 / 2.0;
    return distance;
}

// Capture Thread: saves images to disk
void capture_frames() {
    while (!stop_threads) {
        if (processing_in_progress) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        float distance = measure_distance();
        std::cout << "Measured distance: " << distance << " cm" << std::endl;

        if (distance < 20.0) { // If object closer than 30 cm
            cv::VideoCapture cap(0);
            // cap.set(cv::CAP_PROP_FRAME_WIDTH, 224);
            // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 224);
            if (!cap.isOpened()) {
                std::cerr << "Error: Could not open camera!" << std::endl;
                return;
            }
            std::cout << "Camera opened successfully. Capturing image..." << std::endl;
            usleep(500000); // Wait 0.5 seconds for camera warm-up
            cv::Mat frame;
            cap >> frame;

            if (!frame.empty()) {
                std::lock_guard<std::mutex> lock(frame_mutex);
                cv::imwrite(saved_image_path, frame);
                frame_ready = true; // Signal that frame is ready
                processing_in_progress = true;  // Signal that we're starting processing
                std::cout << "Object detected! Frame captured and saved!" << std::endl;
            }

            cap.release();
            // std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // small pause after capture
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Check distance every 100ms
        }
    }
}



// Function to run Python script and get classification
std::string run_python_script(const std::string& image_file) {
    std::string cmd = "/home/abhirathkoushik/RTES_files/RTES_final_project/myenv/bin/python3 predict_tflite.py " + image_file;
    std::string result;
    char buffer[256];

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        std::cerr << "Failed to run Python script." << std::endl;
        return "";
    }

    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }

    return result;
}

// Simple JSON field extractor
std::string extract_json_field(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    size_t start = json.find_first_not_of(" \":", pos);
    size_t end = json.find_first_of(",}", start);
    std::string field = json.substr(start, end - start);
    field.erase(remove(field.begin(), field.end(), '\"'), field.end());
    return field;
}

// Inference Thread: waits for saved frame and runs inference
void run_inference() {
    while (!stop_threads) {
        if (frame_ready) {
            {
                std::lock_guard<std::mutex> lock(frame_mutex);
                frame_ready = false; // Reset ready flag
            }

            std::cout << "Running inference on saved frame..." << std::endl;
            std::string output = run_python_script(saved_image_path);

            if (!output.empty()) {
                // Clean output
                output.erase(std::remove_if(output.begin(), output.end(), ::isspace), output.end());

                std::string detected_class = extract_json_field(output, "class");
                std::string confidence = extract_json_field(output, "confidence");
                std::string inference_time = extract_json_field(output, "inference_time_ms");

                if (detected_class == "biodegradable") {
                    std::cout << "Detected class: Biodegradable" << std::endl;
                    sweep_servo_1();
                } else if (detected_class == "nonbiodegradable") {
                    std::cout << "Detected class: Non-Biodegradable" << std::endl;
                    sweep_servo_2();
                } else {
                    std::cout << "Unknown detection result!" << std::endl;
                }

                processing_in_progress = false;

                std::cout << "Confidence: " << confidence << std::endl;
                std::cout << "Inference Time: " << inference_time << " ms" << std::endl;
            } else {
                std::cerr << "Error: No output from Python!" << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // avoid busy-waiting
    }
}

int main() {
    setup_gpio();
    init_servos();
    set_servo2_initial();
    set_servo1_initial();

    std::thread capture_thread(capture_frames);
    std::thread inference_thread(run_inference);

    std::cout << "Press ENTER to stop..." << std::endl;
    std::cin.get();

    stop_threads = true;

    capture_thread.join();
    inference_thread.join();

    std::cout << "Threads stopped. Exiting." << std::endl;
    return 0;
}
