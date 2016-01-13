
CXXFLAGS := -std=c++11 -Wall -g -O2 -L/usr/local/Cellar/boost/1.59.0/lib
CC = g++-5
TARGET = compute.cpp
all: compute

compute: compute.cpp
	$(CC) $(CXXFLAGS) -I /usr/local/include -lboost_system $(TARGET) -o compute

clean:
	rm compute
