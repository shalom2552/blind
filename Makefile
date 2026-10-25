VERSION := $(shell git describe --abbrev=0 --always --tags 2>/dev/null || echo unknown)
CFLAGS  += -std=c99 -DVERSION=\"$(VERSION)\"

TRG := blind
SRC := blind.c

all: $(SRC)
	$(CC) $(CFLAGS) -o $(TRG) $(SRC)

check: $(SRC)
	$(CC) $(SRC) -o check-native -std=c99 -Wall -Wextra -Wpedantic
	$(RM) check-native
	@command -v clang >/dev/null || { echo "clang not found" && exit 1; }
	clang $(SRC) -o check-native -std=c99 -Wall -Wextra -Wpedantic
	$(RM) check-native

cross-compile: $(SRC)
	@command -v zig >/dev/null || { echo "zig not found" && exit 1; }
	@for target in x86_64-linux-musl aarch64-linux-musl x86_64-macos aarch64-macos; do \
		$(MAKE) --no-print-directory CC="zig cc -target $$target" TRG=$$target || exit 1; \
		$(RM) $$target; \
	done

clean:
	$(RM) $(TRG)
