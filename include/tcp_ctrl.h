#pragma once

extern bool tcp_ctrl_init(void);
extern void tcp_ctrl_sendToAllClients(char data[], int len);