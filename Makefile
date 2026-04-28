CC_INSTR     := afl-clang-fast
CC_VANILLA   := gcc

CFLAGS_INSTR   := -fsanitize=address -g -O1
CFLAGS_VANILLA := -g -O1

INSTALL_DIR         := /build/install
INSTALL_DIR_VANILLA := /build/install_vanilla

LDFLAGS_INSTR   := -fsanitize=address -lz -lm
LDFLAGS_VANILLA := -lz -lm

.PHONY: build build-qemu build-persistent fuzz fuzz-qemu fuzz-persistent clean

build:
	$(CC_INSTR) $(CFLAGS_INSTR) src/harness.c \
		-I$(INSTALL_DIR)/include \
		$(INSTALL_DIR)/lib/libpng12.a \
		$(LDFLAGS_INSTR) \
		-o png_fuzz

build-qemu:
	$(CC_VANILLA) $(CFLAGS_VANILLA) src/harness.c \
		-I$(INSTALL_DIR_VANILLA)/include \
		$(INSTALL_DIR_VANILLA)/lib/libpng12.a \
		$(LDFLAGS_VANILLA) \
		-o png_fuzz_qemu

build-persistent:
	$(CC_INSTR) $(CFLAGS_INSTR) src/harness_persistent.c \
		-I$(INSTALL_DIR)/include \
		$(INSTALL_DIR)/lib/libpng12.a \
		$(LDFLAGS_INSTR) \
		-o png_fuzz_persistent


fuzz: build
	mkdir -p findings
	afl-fuzz -i seeds -o findings -x dictionaries/png.dict -- ./png_fuzz @@

fuzz-qemu: build-qemu
	mkdir -p findings-qemu
	afl-fuzz -Q -i seeds -o findings-qemu -x dictionaries/png.dict -- ./png_fuzz_qemu @@

fuzz-persistent: build-persistent
	mkdir -p findings-persistent
	afl-fuzz -i seeds -o findings-persistent -x dictionaries/png.dict -- ./png_fuzz_persistent @@

clean:
	rm -f png_fuzz png_fuzz_qemu png_fuzz_persistent
	rm -rf findings findings-qemu findings-persistent
