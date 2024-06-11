#pragma once

typedef struct
{
  char data[256];
  int len;
} tcp_tx_msg_s;

extern void tcpTxTask(void *param);