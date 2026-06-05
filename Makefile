.PHONY: all build configure clean test bench rebuild

BUILD_DIR := build
CMAKE_FLAGS := -G Ninja -DCMAKE_BUILD_TYPE=Release

all: build

configure:
	cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR) -j$(shell nproc)

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

bench: build
	./$(BUILD_DIR)/order_book_bench

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build
