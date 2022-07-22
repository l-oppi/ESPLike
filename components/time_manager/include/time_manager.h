#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <time.h>
#include <unistd.h>

uint32_t time_seconds();
uint32_t time_millis();
uint32_t time_micros();
uint32_t time_nanos();
uint32_t time_sleep(uint32_t seconds);
int time_msleep(useconds_t msec);
int time_usleep(useconds_t usec);
int time_nanosleep(const struct timespec *req, struct timespec *rem);
