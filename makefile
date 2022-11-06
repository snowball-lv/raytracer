
BIN = bin/raytracer
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=out/%.o)
DEPS = $(SRCS:src/%.c=out/%.d)

CFLAGS = -c -MMD -I inc -Wall -O2
LDFLAGS = -lm

all: $(BIN)

-include $(DEPS)

out:
	mkdir out

out/%.o: src/%.c | out
	$(CC) $(CFLAGS) $< -o $@

bin:
	mkdir bin

$(BIN): $(OBJS) | bin
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -rf out bin

test: all
	$(BIN) scene.conf
