#include "capture_image_non_block.hpp"
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <iostream>
#include <errno.h>

#define WIDTH 640
#define HEIGHT 480
#define NUM_BUFFERS 4


bool capture_image(const std::string& filename, const std::string& device)
{
    int fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return false;
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        close(fd);
        return false;
    }

    v4l2_requestbuffers req{};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return false;
    }

    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        perror("VIDIOC_QUERYBUF");
        close(fd);
        return false;
    }

    void* buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return false;
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF");
        munmap(buffer, buf.length);
        close(fd);
        return false;
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        munmap(buffer, buf.length);
        close(fd);
        return false;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timeval tv = {2, 0};
    if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) {
        std::cerr << "Timeout waiting for frame\n";
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        munmap(buffer, buf.length);
        close(fd);
        return false;
    }

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("VIDIOC_DQBUF");
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        munmap(buffer, buf.length);
        close(fd);
        return false;
    }

    cv::Mat yuyv(HEIGHT, WIDTH, CV_8UC2, buffer);
    cv::Mat bgr;
    cv::cvtColor(yuyv, bgr, cv::COLOR_YUV2BGR_YUYV);
    cv::imwrite(filename, bgr);

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    munmap(buffer, buf.length);
    close(fd);
    return true;
}
