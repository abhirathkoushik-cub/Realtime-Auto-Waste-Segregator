# Makefile for compiling sequencer_threaded_system.cpp with C++20 and jthreads

# Compiler
CXX = g++

MAIN = final.cpp
# To debug, change to:
#MAIN = temp_test_final.cpp

# Source and Target
SRC = $(MAIN) servo.cpp ads1115rpi.cpp capture_image_non_block.cpp
HDR = servo.hpp Sequencer.hpp ads1115rpi.h capture_image_non_block.hpp persistent_v4l2_camera.hpp
TARGET = sequencer_system

# Compiler and linker flags
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -lv4l2 -w
LDFLAGS = -lwiringPi -lgpiod -lrt -pthread 
OPENCV_FLAGS = `pkg-config --cflags --libs opencv4`

# Compilation Rule
$(TARGET): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(OPENCV_FLAGS) $(LDFLAGS)

run: $(TARGET)
	sudo ./$(TARGET)

# Clean Rule
clean:
	rm -f $(TARGET)
