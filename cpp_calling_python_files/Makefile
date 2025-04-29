# Makefile for compiling call_python_from_cpp_script.cpp

# Compiler
CXX = g++

# Source and Target
SRC = call_python_from_cpp_script.cpp
TARGET = image_classify

# OpenCV flags (autodetected)
OPENCV_FLAGS = `pkg-config --cflags --libs opencv4`

# Compilation Rule
$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(OPENCV_FLAGS)

# Clean Rule
clean:
	rm -f $(TARGET)
	