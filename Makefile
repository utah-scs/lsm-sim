CXX := g++-7
# CXX := g++
CXXFLAGS := -std=c++14 -Wall -g -pedantic-errors -Werror -O3 \
						-Wno-unused-parameter -Wimplicit-fallthrough=0 -Wextra -Weffc++
# REPLAY := yes enables a policy that can feed traces to memcached, but it
# requires libmemcached to be linked in as a result. Only enable it if you
# have the library and headers installed.
REPLAY ?= no

LDFLAGS := -lcrypto -lssl -lpython2.7

ifeq ($(REPLAY),yes)
LDFLAGS += -lmemcached 
else
CXXFLAGS += -DNOREPLAY
endif

HEADERS := $(wildcard src/*.h)
SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, src/%.o, $(SRCS))

all: lsm-sim

%.o: src/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $<

lsm-sim: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ 

clean:
	-rm lsm-sim src/*.o

debug: CXXFLAGS += -DDEBUG
debug: lsm-sim
