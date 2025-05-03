# Makefile for compiling call_python_from_cpp_script.cpp

# Compiler
CXX = g++

# Source and Target
SRC = servo_threads_script.cpp servo.cpp
HDR = servo.hpp
TARGET = image_servo_thread_classify

# OpenCV flags (autodetected)
OPENCV_FLAGS = `pkg-config --cflags --libs opencv4`
WIRING_PI_THREAD_FLAGS = -lwiringPi -pthread

# Compilation Rule
$(TARGET): $(SRC) $(HDR)
	$(CXX) $(SRC) -o $(TARGET) $(OPENCV_FLAGS) $(WIRING_PI_THREAD_FLAGS)
 
run: $(TARGET)
	sudo ./$(TARGET)

# Clean Rule
clean:
	rm -f $(TARGET)
	