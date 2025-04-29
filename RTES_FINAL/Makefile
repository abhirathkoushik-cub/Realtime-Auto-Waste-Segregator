CXX = g++
CXXFLAGS = --std=c++23 -Wall -Werror -pedantic
LDFLAGS = -lpigpio -lrt -pthread

TARGET = service_app
SRC = servo.cpp

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
