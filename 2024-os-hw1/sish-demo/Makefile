CC = gcc
CFLAGS = -Wall -Wextra -g
INCLUDES = -Iheader
SRC_DIR = src
OBJ_DIR = obj

# 모든 .c 파일을 찾습니다
SRCS = $(wildcard $(SRC_DIR)/*.c) single-cycle.c
# .c 파일을 .o 파일로 변환합니다
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)

TARGET = mips_simulator

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean run

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

run: $(TARGET)
	./$(TARGET) test_prog/simple.bin
