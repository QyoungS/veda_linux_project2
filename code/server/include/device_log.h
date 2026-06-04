#ifndef DEVICE_LOG_H
#define DEVICE_LOG_H

#define DEVICE_DEBUG 0

#if DEVICE_DEBUG
#define DEVICE_LOG(...) printf(__VA_ARGS__)
#else
#define DEVICE_LOG(...) do { } while (0)
#endif

#endif
