CC_INSTR     := afl-clang-fast
CC_VANILLA   := gcc

CFLAGS_INSTR   := -fsanitize=address -g -O1
CFLAGS_NO_ASAN := -g -O1
CFLAGS_VANILLA := -g -O1

INSTALL_DIR         := /build/install
INSTALL_DIR_NO_ASAN := /build/install_no_asan
INSTALL_DIR_VANILLA := /build/install_vanilla

LDFLAGS_INSTR   := -fsanitize=address -lz -lm
LDFLAGS_NO_ASAN := -lz -lm
LDFLAGS_VANILLA := -lz -lm

PARALLEL ?= 8
AFL_ENV ?= AFL_SKIP_CPUFREQ=1 ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:symbolize=0
AFL_PARALLEL_ENV ?= AFL_NO_UI=1 $(AFL_ENV)
PNG_DICT ?= /build/dictionaries/png.dict

.PHONY: build build-no-asan build-qemu build-persistent build-progressive build-api build-write-api build-metadata-api fuzz fuzz-no-asan fuzz-qemu fuzz-persistent fuzz-progressive fuzz-api fuzz-write-api fuzz-metadata-api fuzz-parallel fuzz-qemu-parallel fuzz-persistent-parallel fuzz-progressive-parallel fuzz-api-parallel fuzz-write-api-parallel fuzz-metadata-api-parallel q3-yields q8-edge-counts q8-speed clean

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

build-no-asan:
	$(CC_INSTR) $(CFLAGS_NO_ASAN) src/harness.c \
		-I$(INSTALL_DIR_NO_ASAN)/include \
		$(INSTALL_DIR_NO_ASAN)/lib/libpng12.a \
		$(LDFLAGS_NO_ASAN) \
		-o png_fuzz_no_asan

build-persistent:
	$(CC_INSTR) $(CFLAGS_INSTR) src/harness_persistent.c \
		-I$(INSTALL_DIR)/include \
		$(INSTALL_DIR)/lib/libpng12.a \
		$(LDFLAGS_INSTR) \
		-o png_fuzz_persistent

build-progressive:
	$(CC_INSTR) $(CFLAGS_INSTR) src/harness_progressive.c \
		-I$(INSTALL_DIR)/include \
		$(INSTALL_DIR)/lib/libpng12.a \
		$(LDFLAGS_INSTR) \
		-o png_fuzz_progressive

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
	$(AFL_ENV) afl-fuzz -i seeds -o findings -x $(PNG_DICT) -- ./png_fuzz @@

fuzz-no-asan: build-no-asan
	mkdir -p findings-no-asan
	$(AFL_ENV) afl-fuzz -i seeds -o findings-no-asan -x $(PNG_DICT) -- ./png_fuzz_no_asan @@

fuzz-qemu: build-qemu
	mkdir -p findings-qemu
	$(AFL_ENV) afl-fuzz -Q -i seeds -o findings-qemu -x $(PNG_DICT) -- ./png_fuzz_qemu @@

fuzz-persistent: build-persistent
	mkdir -p findings-persistent
	$(AFL_ENV) afl-fuzz -i seeds -o findings-persistent -x $(PNG_DICT) -- ./png_fuzz_persistent @@

fuzz-progressive: build-progressive
	mkdir -p findings-progressive
	$(AFL_ENV) afl-fuzz -i seeds -o findings-progressive -x $(PNG_DICT) -- ./png_fuzz_progressive @@

fuzz-api: build-api
	mkdir -p findings-api
	$(AFL_ENV) afl-fuzz -i seeds-api -o findings-api -- ./png_fuzz_api @@

fuzz-write-api: build-write-api
	mkdir -p findings-write-api
	$(AFL_ENV) afl-fuzz -i seeds-write -o findings-write-api -- ./png_fuzz_write_api @@

fuzz-metadata-api: build-metadata-api
	mkdir -p findings-metadata-api
	$(AFL_ENV) afl-fuzz -i seeds-metadata -o findings-metadata-api -- ./png_fuzz_metadata_api @@

fuzz-parallel: build
	mkdir -p findings
	@echo "Starting $(PARALLEL) AFL++ workers in findings"
	@$(AFL_PARALLEL_ENV) afl-fuzz -i seeds -o findings -x $(PNG_DICT) -M main -- ./png_fuzz @@ & \
	for i in $$(seq 2 $(PARALLEL)); do \
		$(AFL_PARALLEL_ENV) afl-fuzz -i seeds -o findings -x $(PNG_DICT) -S worker$$i -- ./png_fuzz @@ & \
	done; \
	wait

fuzz-qemu-parallel: build-qemu
	mkdir -p findings-qemu
	@echo "Starting $(PARALLEL) AFL++ QEMU workers in findings-qemu"
	@$(AFL_PARALLEL_ENV) afl-fuzz -Q -i seeds -o findings-qemu -x $(PNG_DICT) -M main -- ./png_fuzz_qemu @@ & \
	for i in $$(seq 2 $(PARALLEL)); do \
		$(AFL_PARALLEL_ENV) afl-fuzz -Q -i seeds -o findings-qemu -x $(PNG_DICT) -S worker$$i -- ./png_fuzz_qemu @@ & \
	done; \
	wait

fuzz-persistent-parallel: build-persistent
	mkdir -p findings-persistent
	@echo "Starting $(PARALLEL) AFL++ persistent workers in findings-persistent"
	@$(AFL_PARALLEL_ENV) afl-fuzz -i seeds -o findings-persistent -x $(PNG_DICT) -M main -- ./png_fuzz_persistent @@ & \
	for i in $$(seq 2 $(PARALLEL)); do \
		$(AFL_PARALLEL_ENV) afl-fuzz -i seeds -o findings-persistent -x $(PNG_DICT) -S worker$$i -- ./png_fuzz_persistent @@ & \
	done; \
	wait

fuzz-progressive-parallel: build-progressive
	mkdir -p findings-progressive
	@echo "Starting $(PARALLEL) AFL++ progressive workers in findings-progressive"
	@$(AFL_PARALLEL_ENV) afl-fuzz -i seeds -o findings-progressive -x $(PNG_DICT) -M main -- ./png_fuzz_progressive @@ & \
	for i in $$(seq 2 $(PARALLEL)); do \
		$(AFL_PARALLEL_ENV) afl-fuzz -i seeds -o findings-progressive -x $(PNG_DICT) -S worker$$i -- ./png_fuzz_progressive @@ & \
	done; \
	wait

fuzz-api-parallel: build-api
	mkdir -p findings-api
	@echo "Starting $(PARALLEL) AFL++ workers in findings-api"
	@$(AFL_PARALLEL_ENV) afl-fuzz -i seeds-api -o findings-api -M main -- ./png_fuzz_api @@ & \
	for i in $$(seq 2 $(PARALLEL)); do \
		$(AFL_PARALLEL_ENV) afl-fuzz -i seeds-api -o findings-api -S worker$$i -- ./png_fuzz_api @@ & \
	done; \
	wait

fuzz-write-api-parallel: build-write-api
	mkdir -p findings-write-api
	@echo "Starting $(PARALLEL) AFL++ workers in findings-write-api"
	@$(AFL_PARALLEL_ENV) afl-fuzz -i seeds-write -o findings-write-api -M main -- ./png_fuzz_write_api @@ & \
	for i in $$(seq 2 $(PARALLEL)); do \
		$(AFL_PARALLEL_ENV) afl-fuzz -i seeds-write -o findings-write-api -S worker$$i -- ./png_fuzz_write_api @@ & \
	done; \
	wait

fuzz-metadata-api-parallel: build-metadata-api
	mkdir -p findings-metadata-api
	@echo "Starting $(PARALLEL) AFL++ workers in findings-metadata-api"
	@$(AFL_PARALLEL_ENV) afl-fuzz -i seeds-metadata -o findings-metadata-api -M main -- ./png_fuzz_metadata_api @@ & \
	for i in $$(seq 2 $(PARALLEL)); do \
		$(AFL_PARALLEL_ENV) afl-fuzz -i seeds-metadata -o findings-metadata-api -S worker$$i -- ./png_fuzz_metadata_api @@ & \
	done; \
	wait

q3-yields:
	python3 scripts/extract_q3_yields.py --output evidence/q3-yields/README.md

q8-edge-counts:
	python3 scripts/measure_q8_edges.py

q8-speed:
	python3 scripts/measure_q8_speed.py

clean:
	rm -f png_fuzz png_fuzz_no_asan png_fuzz_qemu png_fuzz_persistent png_fuzz_progressive png_fuzz_api png_fuzz_write_api png_fuzz_metadata_api
	rm -rf findings findings-no-asan findings-qemu findings-persistent findings-progressive findings-api findings-write-api findings-metadata-api
