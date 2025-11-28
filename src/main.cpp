#include "Config.hpp"
#include "Audio.hpp"
#include "Graphics.hpp"
#include "USB.hpp"

#include <thread>
#include <iostream>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s <usb-device>\n", argv[0]);
        return 1;
    }

    try {
        Audio audio;
        Graphics gfx(audio);

        USB usb(argv[1]);
        std::thread usbThread([&]() {
            usb.start([&](uint8_t, uint8_t* data, uint8_t count){
                if (count < 4) return;
                switch (data[0]) {
                    case 0x08: // note off
                        break;
                    case 0x09: { // note on
                        int key = data[2];
                        int vel = data[3];
                        if (vel > 0) {
                            audio.noteOn(static_cast<uint8_t>(key), static_cast<uint8_t>(vel));
                        }
                    } break;
                    case 0x0E: { // pitch
                        audio.pitchBend(data[3]);
                    } break;
                    default:
                        std::printf("%02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);
                }
            });
        });

        usbThread.detach();

        std::thread decayThread([&]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                audio.decayKeysOnce(10);
            }
        });

        decayThread.detach();

        gfx.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
