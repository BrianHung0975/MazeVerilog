VERILATOR := verilator
RTL       := rtl/maze_32x32_top.v
TOP       := maze_32x32_top

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LDFLAGS := $(shell pkg-config --libs sdl2)

.PHONY: all sim run test lint clean

all: sim

sim: build/maze_sim

build/maze_sim: $(RTL) sim/sim_sdl.cpp
	@mkdir -p build/obj_dir_sim
	$(VERILATOR) --cc --exe --build --trace -j 0 \
		--Mdir build/obj_dir_sim \
		--top-module $(TOP) \
		-GSIM_SHIFT=8 \
		-Wno-UNUSEDSIGNAL \
		-CFLAGS "-O2 -std=c++17 $(SDL_CFLAGS)" \
		-LDFLAGS "$(SDL_LDFLAGS)" \
		-o ../maze_sim \
		$(RTL) sim/sim_sdl.cpp

run: sim
	./build/maze_sim

test: build/maze_tb
	./build/maze_tb

build/maze_tb: $(RTL) sim/tb_maze.cpp
	@mkdir -p build/obj_dir_tb
	$(VERILATOR) --cc --exe --build --trace -j 0 \
		--Mdir build/obj_dir_tb \
		--top-module $(TOP) \
		-GSIM_SHIFT=12 \
		-Wno-UNUSEDSIGNAL \
		-CFLAGS "-O2 -std=c++17" \
		-o ../maze_tb \
		$(RTL) sim/tb_maze.cpp

lint:
	$(VERILATOR) --lint-only -Wall -Wno-UNUSEDSIGNAL $(RTL)

clean:
	rm -rf build
