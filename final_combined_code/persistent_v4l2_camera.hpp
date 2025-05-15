// persistent_v4l2_camera.hpp
#ifndef PERSISTENT_V4L2_CAMERA_HPP
#define PERSISTENT_V4L2_CAMERA_HPP

#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <stdexcept>
#include <cstring>
#include <iostream>

class PersistentV4L2Camera {
public:
    PersistentV4L2Camera(const std::string& device = "/dev/video0", int width = 640, int height = 480)
        : fd(-1), buffer(nullptr), buffer_length(0), WIDTH(width), HEIGHT(height)
    {
        open_device(device);
    }

    ~PersistentV4L2Camera() {
        if (fd >= 0) {
            v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(fd, VIDIOC_STREAMOFF, &type);
            if (buffer) munmap(buffer, buffer_length);
            close(fd);
        }
    }

    bool captureToFile(const std::string& filename) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = 0;

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) return false;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeval tv = {2, 0};

        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) {
            std::cerr << "Timeout waiting for frame\n";
            return false;
        }

        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) return false;

        cv::Mat yuyv(HEIGHT, WIDTH, CV_8UC2, buffer);
        cv::Mat bgr;
        cv::cvtColor(yuyv, bgr, cv::COLOR_YUV2BGR_YUYV);
        return cv::imwrite(filename, bgr);
    }

private:
    int fd;
    void* buffer;
    size_t buffer_length;
    const int WIDTH, HEIGHT;

    void open_device(const std::string& device) {
        fd = open(device.c_str(), O_RDWR);
        if (fd < 0) throw std::runtime_error("Failed to open device");

        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = WIDTH;
        fmt.fmt.pix.height = HEIGHT;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;

        if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
            throw std::runtime_error("VIDIOC_S_FMT failed");

        v4l2_requestbuffers req{};
        req.count = 1;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
            throw std::runtime_error("VIDIOC_REQBUFS failed");

        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = 0;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0)
            throw std::runtime_error("VIDIOC_QUERYBUF failed");

        buffer_length = buf.length;
        buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffer == MAP_FAILED)
            throw std::runtime_error("mmap failed");

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("VIDIOC_STREAMON failed");
    }
};

#endif
