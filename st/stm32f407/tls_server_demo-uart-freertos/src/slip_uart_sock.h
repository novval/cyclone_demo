#ifndef SLIP_UART_SOCK_H
#define SLIP_UART_SOCK_H

#include <stdint.h>
#include "memblock.h"
#include "error.h"
#include "os_port.h"

#define SLIP_SOCK_MAX   2

typedef void *SlipSock_t;

typedef enum {
    escConnect,
    escData,
    escDisconnect,
} TSockCmd;

#pragma pack(push, 1)
	typedef struct {
		TSockCmd	cmd;
		uint32_t	id;
		uint16_t	len;
		uint8_t		data[];
		// uint16_t	crc; --
	} TSockHeader;
#pragma pack(pop)

void slip_sock_init(void);
SlipSock_t *slip_sock_new(void);
void slip_sock_free(SlipSock_t *sock);
SlipSock_t *slip_sock_get(uint32_t id);
SlipSock_t *slip_sock_alloc(uint32_t id);
BaseType_t slip_sock_connect(uint32_t id);
SlipSock_t *slip_sock_wait_connect(void);
error_t slip_sock_recv(SlipSock_t *sock, uint8_t *data, size_t size, size_t *received);
error_t slip_sock_push_recv_data_in_queue(SlipSock_t *sock, TMemBlock *block);
error_t slip_sock_send(SlipSock_t sock, const void *data, size_t length, size_t *written);
error_t slip_sock_set_timeout(SlipSock_t *sock, uint16_t timeout);
error_t slip_sock_close(SlipSock_t *sock);

#endif  // SLIP_UART_SOCK_H
