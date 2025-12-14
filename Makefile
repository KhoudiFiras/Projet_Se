CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -Iinclude `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -ldl

SRC = src/gui.c src/parser.c src/policies_loader.c
OBJ = $(SRC:.c=.o)


POLICY_SRC = $(wildcard politiques/*.c)


POLICY_SO = $(POLICY_SRC:.c=.so)

TARGET = scheduler

all: $(POLICY_SO) $(TARGET)


$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)


politiques/%.so: politiques/%.c
	$(CC) -shared -fPIC -o $@ $<


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) politiques/*.so

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run



