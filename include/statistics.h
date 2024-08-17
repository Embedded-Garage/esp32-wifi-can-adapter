#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t total_rcvd_can_frames;
    uint32_t lost_frames_can_to_app;
    uint32_t lost_frames_app_to_can;
    uint32_t lost_frames_app_to_log;
    uint32_t lost_frames_log_to_tcp;
    uint32_t lost_frames_log_to_serial;
    uint32_t lost_frames_tcp_to_app;
    uint32_t lost_frames_serial_to_app;
} statistics_s;

extern statistics_s statistics;