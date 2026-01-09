CC        := clang
CFLAGS    := -std=c23 -Wall -Wextra -Wshadow -Wfloat-equal -Werror
BUILD_DIR := build

.PHONY: all
all: $(BUILD_DIR)/vm_stat2 $(BUILD_DIR)/vm_stat2-dev

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: test
test: $(BUILD_DIR)/vm_stat2
	@bash test.sh $(BUILD_DIR)/vm_stat2

$(BUILD_DIR)/vm_stat2: main.c | $(BUILD_DIR)
	@$(CC) -o $@ $< $(CFLAGS) -O2 -DNDEBUG

$(BUILD_DIR)/vm_stat2-dev: main.c | $(BUILD_DIR)
	@$(CC) -o $@ $< $(CFLAGS) -g -O0

$(BUILD_DIR):
	@mkdir -p $@
