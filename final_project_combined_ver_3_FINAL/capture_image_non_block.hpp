// capture_image_non_block.hpp
#ifndef CAPTURE_IMAGE_NON_BLOCK_HPP
#define CAPTURE_IMAGE_NON_BLOCK_HPP

#include<string>

bool capture_image(const std::string& filename, const std::string& device = "/dev/video0"); 
//bool init_camera(const std::string& device = "/dev/video0", int width = 640, int height = 480);
// bool capture_v4l2_frame(const std::string& device = "/dev/video0", const std::string& filename = "capture.jpg");
//oid close_camera();
// bool imageCaptureInit(const std::string& device = "/dev/video0", int width = 640, int height = 480);
// bool imageCaptureService(const std::string& filename = "capture.jpg");

#endif // CAPTURE_IMAGE_NON_BLOCK_HPP
