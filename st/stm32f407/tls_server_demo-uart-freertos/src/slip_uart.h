#ifndef SLIP_UART_H_INCLUDED
#define SLIP_UART_H_INCLUDED

#include "os_port.h"

bool_t SU_Init(void);
void SU_ReceiveTask(void *param);
void SU_TransmitTask(void *param);

#endif  // SLIP_UART_H_INCLUDED
