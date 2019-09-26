VULKAN_SDK_PATH = ./ext/vulkan-sdk-1.1.121.1/x86_64
CFLAGS = -std=c++17 -I$(VULKAN_SDK_PATH)/include
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

.PHONY: test clean

all: src/main.cpp
	clang++ $(CFLAGS) -o vulkan-test src/main.cpp $(LDFLAGS)

clean:
	rm -f vulkan-test

test: all
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/vulkan/explicit_layer.d ./vulkan-test
