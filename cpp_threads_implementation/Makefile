# Makefile for compiling call_python_from_cpp_script.cpp

# Compiler
CXX = g++

# Source and Target
SRC = call_threads_script.cpp
TARGET = image_thread_classify

# OpenCV flags (autodetected)
OPENCV_FLAGS = `pkg-config --cflags --libs opencv4`
WIRING_PI_THREAD_FLAGS = -lwiringPi -pthread

# Compilation Rule
$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(OPENCV_FLAGS) $(WIRING_PI_THREAD_FLAGS)

# Clean Rule
clean:
	rm -f $(TARGET)
	