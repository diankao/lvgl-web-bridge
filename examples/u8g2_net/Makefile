# net_hello — u8g2 无头网络示例 (无 SDL 依赖, 复用 LVGL Web Bridge 协议)
CFLAGS = -g -Wall -I../../../csrc/.

SRC = $(shell ls ../../../csrc/*.c) ../../bitmap/common/u8x8_d_bitmap.c main.c
OBJ = $(SRC:.c=.o)

u8g2_net: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o u8g2_net

clean:
	-rm $(OBJ) u8g2_net

.PHONY: clean
