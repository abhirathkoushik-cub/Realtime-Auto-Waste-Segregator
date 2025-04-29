#include <iostream>
#include <cstdio>    // for popen(), fgets()
#include <memory>    // for std::unique_ptr
#include <string>
#include <sstream>   // for istringstream
#include <algorithm> // for remove_if
#include <opencv2/opencv.hpp>
using namespace std;

// Function to run the Python script and capture its output
std::string run_python_script(const std::string& image_file) {
    std::string cmd = "python3 predict_tflite.py " + image_file;
    std::string result;
    char buffer[256];

    // Open a pipe to run the command
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        std::cerr << "Failed to run Python script." << std::endl;
        return "";
    }

    // Read the output from the Python script
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }

    return result;
}

// Simple manual JSON field extractor
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

// Function to capture image from USB camera
bool capture_image(const std::string& output_filename) {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera!" << std::endl;
        return false;
    }

    std::cout << "Camera opened successfully. Capturing image..." << std::endl;

    cv::Mat frame;
    // cv::waitKey(1000); // optional warmup delay
    cap >> frame;

    if (frame.empty()) {
        std::cerr << "Error: Captured empty frame!" << std::endl;
        return false;
    }

    bool success = cv::imwrite(output_filename, frame);
    cap.release();

    if (!success) {
        std::cerr << "Error: Could not save captured image!" << std::endl;
        return false;
    }

    std::cout << "Image captured and saved as " << output_filename << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    std::string image;

    if (argc == 2) {
        image = argv[1];
        std::cout << "Using provided image: " << image << std::endl;
    } else {
        image = "capture.jpg";
        std::cout << "No image provided. Capturing from camera..." << std::endl;
        if (!capture_image(image)) {
            std::cerr << "Failed to capture image. Exiting." << std::endl;
            return 1;
        }
    }
    std::string output = run_python_script(image);

    if (output.empty()) {
        std::cerr << "No output from Python script." << std::endl;
        return 1;
    }

    // Remove whitespace (spaces, newlines) from the JSON string
    output.erase(std::remove_if(output.begin(), output.end(), ::isspace), output.end());

    std::cout << "Python JSON output: " << output << std::endl;

    // Parse JSON fields
    std::string detected_class = extract_json_field(output, "class");
    std::string confidence = extract_json_field(output, "confidence");
    std::string inference_time = extract_json_field(output, "inference_time_ms");

    // Check and display the prediction
    if (detected_class == "biodegradable") {
        std::cout << "Detected class: Biodegradable" << std::endl;
    } else if (detected_class == "nonbiodegradable") {
        std::cout << "Detected class: Non-Biodegradable" << std::endl;
    } else {
        std::cout << "Unknown detection result!" << std::endl;
    }

    std::cout << "Confidence: " << confidence << std::endl;
    std::cout << "Inference Time: " << inference_time << " ms" << std::endl;

    return 0;
}
