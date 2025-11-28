#include "USB.hpp"

#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <thread>

#if LINUX
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <linux/usbdevice_fs.h>
    #include <linux/usb/ch9.h>
#endif

USB::USB(const char *dev_path) {
#if LINUX
    if ((m_fd = ::open(dev_path, O_RDWR)) < 0) {
        throw std::runtime_error(std::string("open failed: ") + std::strerror(errno));
    }

    uint8_t buffer[256];
    usbdevfs_ctrltransfer ctrl{};
    ctrl.bRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    ctrl.bRequest = USB_REQ_GET_DESCRIPTOR;
    ctrl.wValue = (USB_DT_CONFIG << 8);
    ctrl.wIndex = 0;
    ctrl.wLength = sizeof(buffer);
    ctrl.data = buffer;

    if (::ioctl(m_fd, USBDEVFS_CONTROL, &ctrl) < 0) {
        ::close(m_fd);
        throw std::runtime_error(std::string("ioctl GET_DESCRIPTOR failed: ") + std::strerror(errno));
    }

    uint32_t offset = 0;
    while (offset + 2 < sizeof(buffer)) {
        uint8_t len = buffer[offset];
        uint8_t type = buffer[offset + 1];

        if (len == 0) break;
        if (offset + len > sizeof(buffer)) break;

        if (type == USB_DT_ENDPOINT && len >= 7) {
            uint8_t address = buffer[offset + 2];

            if (address & 0x80) {
                m_endpoints.push_back({
                    .address = address,
                    .attributes = buffer[offset + 3],
                    .max_packet_size = uint16_t(buffer[offset + 4] | (buffer[offset + 5] << 8))
                });
            }
        }

        offset += len;
    }

    if (m_endpoints.empty()) {
        ::close(m_fd);
        throw std::runtime_error("No endpoints found");
    }
#endif
}

USB::~USB() {
#if LINUX
    ::close(m_fd);
#endif
}

void USB::start(callback_t callback) {
    static uint8_t data[64];

    while (true) {
        for (const auto& ep : m_endpoints) {
#if LINUX
            usbdevfs_bulktransfer bulk{};
            ::memset(&bulk, 0, sizeof(bulk));
            bulk.ep = ep.address;
            bulk.len = ep.max_packet_size;
            bulk.data = data;
            bulk.timeout = 100;

            int ret = ::ioctl(m_fd, USBDEVFS_BULK, &bulk);
            if (ret >= 0) {
                callback(ep.address, data, ret);
            }
#endif
        }
    }
}
