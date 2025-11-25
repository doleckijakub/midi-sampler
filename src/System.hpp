#define LINUX (defined(__linux__) || defined(__LINUX__))
#define WINDOWS (defined(_WIN32) || defined(WIN32) || defined(WIN64) || defined(_WIN64))

#if !LINUX && !WINDOWS
    #error Only windows an linux systems are supported
#endif