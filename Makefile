CXX := g++-4.9
CXXFLAGS := -std=c++11 -Wall -g -pedantic-errors -Werror -O3
CXXFLAGS += -Wno-unused-parameter -Wextra -Weffc++

LDFLAGS :=

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, src/%.o, $(SRCS))

all: compute

%.o : src/%.cpp $(wildcard *.h)
	$(CXX) $(CXXFLAGS) -c $<

compute: $(OBJS)
	$(CXX) -o $@ $^

clean:
	-rm compute *.o

debug: CXXFLAGS += -DDEBUG
debug: compute
