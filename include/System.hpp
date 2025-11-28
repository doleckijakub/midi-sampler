#define LINUX   (__linux__ || __LINUX__)
#define WINDOWS (_WIN32 || WIN32 || WIN64 || _WIN64)

#if !LINUX && !WINDOWS
    #error Only windows an linux systems are supported
#endif