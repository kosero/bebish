TARGET			:= bebish
SRC_DIR 		:= src
INC_DIR     := inc
BUILD_DIR 	:= build
SRCS      	:= $(wildcard $(SRC_DIR)/*.c)
OBJS				:= $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS				:= $(OBJS:.o=.d)

CC					:= cc
STD         := -std=c99

WARNINGS		:= -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
               -Wsign-conversion -Wcast-align -Wformat=2 \
               -Wundef -Wstrict-prototypes -Wmissing-prototypes

CXXFLAGS 		:= -D_POSIX_C_SOURCE=200809L -I$(INC_DIR) -MMD -MP
CFLAGS 			:= $(STD) $(WARNINGS)
LDFLAGS 		:=
LDLIBS			:=

MODE 				?= release

ifeq ($(MODE),debug)
    CFLAGS  += -O0 -g3 -DDEBUG
endif

ifeq ($(MODE),release)
	CFLAGS += -O2 -DNDEBUG
endif

ifeq ($(MODE),asan)
    CFLAGS  += -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address,undefined
endif

.PHONY: all clean debug release asan run format lint help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CXXFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

debug:
	$(MAKE) MODE=debug

release:
	$(MAKE) MODE=release

asan:
	$(MAKE) MODE=asan

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

format:
	clang-format -i $(SRC_DIR)/*.c

lint:
	clang-tidy $(SRCS) -- $(CXXFLAGS) $(CFLAGS)

help:
	@echo "Available targets:"
	@echo "  make            -> release build"
	@echo "  make debug      -> debug build with -O0 -g3"
	@echo "  make asan       -> build with AddressSanitizer + UBSan"
	@echo "  make run        -> build and run the binary"
	@echo "  make clean      -> remove build directory and binary"
	@echo "  make format     -> format sources with clang-format"
	@echo "  make lint       -> run static analysis with clang-tidy"

-include $(DEPS)

