# Local builds. The submission is bot.cpp itself (tools/pack.py only strips the LOCAL blocks).
CXX      ?= c++
CXXFLAGS ?= -O3 -std=c++17 -march=native

all: build/bot build/referee

build/bot: bot.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -DLOCAL -o $@ $<

build/referee: tools/referee.cpp bot.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf build out

.PHONY: all clean
