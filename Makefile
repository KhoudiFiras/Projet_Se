CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -Iinclude `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -ldl

SRC = src/gui.c src/parser.c src/policies_loader.c src/main.c
OBJ = $(SRC:.c=.o)

# Detect all .c files inside politiques/
POLICY_SRC = $(wildcard politiques/*.c)

# Replace .c → .so
POLICY_SO = $(POLICY_SRC:.c=.so)

TARGET = scheduler

all: $(POLICY_SO) $(TARGET)

# Build main executable
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# Build .so files for each scheduling policy
politiques/%.so: politiques/%.c
	$(CC) -shared -fPIC -o $@ $<

# Build standard .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) politiques/*.so

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run



