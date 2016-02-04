CXX := g++-4.9
CXXFLAGS := -std=c++11 -Wall -g -pedantic-errors -Werror -O2
CXXFLAGS += -Wno-unused-parameter -Wextra -Weffc++

LDFLAGS := -L/usr/local/Cellar/boost/1.59.0/lib -lboost_system

SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp, %.o, $(SRCS))

all: compute

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $<

compute: $(OBJS)
	$(CXX) -lboost_system -o $@ $^ 

clean:
	-rm compute *.o

debug: CXXFLAGS += -DDEBUG
debug: compute
