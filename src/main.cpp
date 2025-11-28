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
                    case 0x28: // percussion off
                        break;
                    case 0x29: { // percussion on
                        uint8_t idx;

                        switch (data[2]) {
                            case 0x28: idx = 0; break;
                            case 0x29: idx = 1; break;
                            case 0x2a: idx = 2; break;
                            case 0x2b: idx = 3; break;
                            case 0x30: idx = 4; break;
                            case 0x31: idx = 5; break;
                            case 0x32: idx = 6; break;
                            case 0x33: idx = 7; break;
                            default: goto invalid;
                        }

                        audio.percOn(idx, data[3]);

                        invalid:;
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
                audio.decayKeysOnce();
                audio.decayPercOnce();
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
