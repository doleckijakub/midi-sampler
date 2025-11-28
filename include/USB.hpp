#pragma once

#include "System.hpp" // LINUX, WINDOWS

#include <cstdint>
#include <vector>
#include <functional>
#include <string>

struct EndpointInfo {
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
};

class USB {
public:
    using callback_t = std::function<void(uint8_t address, uint8_t *data, uint8_t count)>;

private:
#if LINUX
    int m_fd = 0;
#endif

    std::vector<EndpointInfo> m_endpoints;
    callback_t m_callback;

public:
    explicit USB(const char *dev_path);
    ~USB();

    void start(callback_t callback);
};
