# Makefile for compiling sequencer_threaded_system.cpp

# Compiler
CXX = g++

# Source and Target
SRC = sequencer_threaded_system.cpp servo.cpp
HDR = servo.hpp
TARGET = sequencer_system

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
