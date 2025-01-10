FILES=fizzbuzz boostproxy pipes basiccoro interleaving rangecoro

.PHONY: all
all: $(FILES)

%: %.cpp
	g++ -Wall -fcoroutines -g -o -fno-exceptions -std=c++23 -Wextra -fno-inline -o $@ $<

.PHONY: clean 
clean:
	rm -rfv $(FILES)

