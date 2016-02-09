CXX := g++-4.9
CXXFLAGS := -std=c++11 -Wall -g -pedantic-errors -Werror -O2
CXXFLAGS += -Wno-unused-parameter -Wextra -Weffc++

LDFLAGS :=

SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp, %.o, $(SRCS))

all: compute

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $<

compute: $(OBJS)
	$(CXX) -o $@ $^

clean:
	-rm compute *.o

debug: CXXFLAGS += -DDEBUG
debug: compute
