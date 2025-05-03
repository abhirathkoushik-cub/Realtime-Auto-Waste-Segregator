# Makefile for compiling sequencer_threaded_system.cpp with C++20 and jthreads

# Compiler
CXX = g++

# Source and Target
SRC = final.cpp servo.cpp ads1115rpi.cpp
HDR = servo.hpp Sequencer.hpp ads1115rpi.h
TARGET = sequencer_system

# Compiler and linker flags
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread
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
