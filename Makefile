EXE=fizzbuzz boostproxy pipes basiccoro

.PHONY: all
all: $(EXE)

%: %.cpp
	g++ -Wall -fcoroutines -g -o -fno-exceptions -std=c++23 -Wextra -fno-inline -o $@ $<

.PHONY: clean 
clean:
	rm -rfv $(EXE)

