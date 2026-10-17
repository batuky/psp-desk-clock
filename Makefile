# Host-side build and tests for the pure core. Run inside the dev container:
#   tools/dev.sh make test
CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CORE   := $(wildcard src/core/*.c)
TESTS  := $(wildcard tests/test_*.c)
BIN    := $(patsubst tests/%.c,build/%,$(TESTS))

.PHONY: test core-psp-check update-golden clean

test: $(BIN)
	@for t in $(BIN); do echo "== $$t"; ./$$t || exit 1; done

build/%: tests/%.c $(CORE) $(wildcard src/core/*.h tests/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc/core -Itests -o $@ $< $(CORE)

# Regenerate golden images, then review them by eye before committing.
update-golden: $(BIN)
	@for t in $(BIN); do UPDATE_GOLDEN=1 ./$$t || exit 1; done

# The core must also compile with the PSP toolchain (no hosted-only code).
core-psp-check:
	@mkdir -p build
	@for f in $(CORE); do psp-gcc -std=c11 -Wall -Wextra -Werror -Isrc/core -c $$f -o build/psp-check.o || exit 1; done
	@echo "core compiles with psp-gcc"

clean:
	rm -rf build
