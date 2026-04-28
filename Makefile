CC_INSTR     := afl-clang-fast
CC_VANILLA   := gcc

CFLAGS_INSTR   := -fsanitize=address -g -O1
CFLAGS_VANILLA := -g -O1

INSTALL_DIR         := /build/install
INSTALL_DIR_VANILLA := /build/install_vanilla

LDFLAGS_INSTR   := -fsanitize=address -lz -lm
LDFLAGS_VANILLA := -lz -lm

.PHONY: build build-qemu build-persistent build-api build-write-api build-metadata-api fuzz fuzz-qemu fuzz-persistent fuzz-api fuzz-write-api fuzz-metadata-api clean

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

build-api:
	$(CC_INSTR) $(CFLAGS_INSTR) src/harness_text_api.c \
		-I$(INSTALL_DIR)/include \
		$(INSTALL_DIR)/lib/libpng12.a \
		$(LDFLAGS_INSTR) \
		-o png_fuzz_api

build-write-api:
	$(CC_INSTR) $(CFLAGS_INSTR) src/harness_write_api.c \
		-I$(INSTALL_DIR)/include \
		$(INSTALL_DIR)/lib/libpng12.a \
		$(LDFLAGS_INSTR) \
		-o png_fuzz_write_api

build-metadata-api:
	$(CC_INSTR) $(CFLAGS_INSTR) src/harness_metadata_api.c \
		-I$(INSTALL_DIR)/include \
		$(INSTALL_DIR)/lib/libpng12.a \
		$(LDFLAGS_INSTR) \
		-o png_fuzz_metadata_api


fuzz: build
	mkdir -p findings
	afl-fuzz -i seeds -o findings -x dictionaries/png.dict -- ./png_fuzz @@

fuzz-qemu: build-qemu
	mkdir -p findings-qemu
	afl-fuzz -Q -i seeds -o findings-qemu -x dictionaries/png.dict -- ./png_fuzz_qemu @@

fuzz-persistent: build-persistent
	mkdir -p findings-persistent
	afl-fuzz -i seeds -o findings-persistent -x dictionaries/png.dict -- ./png_fuzz_persistent @@

fuzz-api: build-api
	mkdir -p findings-api
	afl-fuzz -i seeds-api -o findings-api -- ./png_fuzz_api @@

fuzz-write-api: build-write-api
	mkdir -p findings-write-api
	afl-fuzz -i seeds-write -o findings-write-api -- ./png_fuzz_write_api @@

fuzz-metadata-api: build-metadata-api
	mkdir -p findings-metadata-api
	afl-fuzz -i seeds-metadata -o findings-metadata-api -- ./png_fuzz_metadata_api @@

clean:
	rm -f png_fuzz png_fuzz_qemu png_fuzz_persistent png_fuzz_api png_fuzz_write_api png_fuzz_metadata_api
	rm -rf findings findings-qemu findings-persistent findings-api findings-write-api findings-metadata-api
