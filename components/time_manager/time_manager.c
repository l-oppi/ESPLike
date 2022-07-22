#include "time_manager.h"

inline uint32_t time_seconds()
{
    return 1ul * ((1ull * xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000);
}

inline uint32_t time_millis()
{
    return (1ul * xTaskGetTickCount() * portTICK_PERIOD_MS);
}

inline uint32_t time_micros()
{
    return esp_timer_get_time();
}

inline uint32_t time_nanos()
{
    return 1000 * time_micros();
}

inline uint32_t time_sleep(uint32_t seconds)
{
    vTaskDelay((1000ul * seconds) / portTICK_PERIOD_MS);
    return 0;
}

inline int time_msleep(useconds_t msec)
{
    vTaskDelay(msec / portTICK_PERIOD_MS);
    return 0;
}

inline int time_usleep(useconds_t usec)
{
    if (usec / 1000 != 0)
        time_msleep(usec / 1000);
    ets_delay_us(usec % 1000);
    return 0;
}

inline int time_nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (req == NULL)
        return -1;
    return time_usleep(1000000 * req->tv_sec + req->tv_nsec / 1000);
}