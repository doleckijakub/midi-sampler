#include "USB.hpp"

#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s /dev/bus/usb/BBB/DDD\n", argv[0]);
        return 1;
    }
    
    USB usb(argv[1]);

    usb.start([](uint8_t address, uint8_t *data, uint8_t count) {
        std::printf("%d %d %d %d\n", data[0], data[1], data[2], data[3]);
    });
}
