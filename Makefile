CXX := g++-4.9
CXXFLAGS := -std=c++14 -Wall -g -pedantic-errors -Werror -O3 \
						-Wno-unused-parameter -Wextra -Weffc++

LDFLAGS := -lmemcached

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, src/%.o, $(SRCS))

all: lsm-sim

%.o : src/%.cpp $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -c $<

lsm-sim: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

clean:
	-rm lsm-sim src/*.o

debug: CXXFLAGS += -DDEBUG
debug: lsm-sim
