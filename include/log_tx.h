#pragma once

typedef struct
{
  char data[256];
  int len;
} log_tx_msg_s;

extern void logTxTask(void *param);