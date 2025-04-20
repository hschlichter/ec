CC = clang++
CFLAGS = -std=c++20 -O0 -g -Wall -Wextra -MMD -fPIC
LDFLAGS = 
INCLUDE = -I.
OUTDIR = ./out
LDINCLUDE =

ec_tests: ec_tests.cpp 
	$(CC) $< -o $(OUTDIR)/ec_tests $(CFLAGS) $(INCLUDE) $(LDINCLUDE) $(LDFLAGS) `pkg-config -cflags catch2 -libs catch2-with-main`

.PHONY: clean
clean:
	@rm -rf $(OUTDIR)

print-%  : ; @echo $* = $($*)
