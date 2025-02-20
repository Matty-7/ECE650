CXX = g++
CXXFLAGS = -g -Wall -std=c++11
TARGETS = ringmaster player

all: $(TARGETS)

ringmaster: ringmaster.cpp function.cpp potato.cpp potato.h function.h
	$(CXX) $(CXXFLAGS) -o $@ ringmaster.cpp function.cpp potato.cpp

player: player.cpp function.cpp potato.cpp potato.h function.h
	$(CXX) $(CXXFLAGS) -o $@ player.cpp function.cpp potato.cpp

clean:
	rm -rf *.o $(TARGETS)

.PHONY: all clean
