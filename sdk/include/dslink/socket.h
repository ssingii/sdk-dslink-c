#ifndef SDK_DSLINK_C_SOCKET_H
#define SDK_DSLINK_C_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef struct SslSocket SslSocket;
typedef struct Socket Socket;

/**
 * \brief          Connects to a designated server. All parameters must be
 *                 initialized otherwise the behavior is undefined.
 *
 * \param sock     An initialized socket. It must not be connected to any
 *                 server. Using an already connected socket will result
 *                 in undefined behavior.
 * \param address  Address of the server.
 * \param port     Port of the server.
 * \param secure   Whether the connection to the server is over SSL or not.
 *
 * \return         0 on success, otherwise an error has occurred.
 */
int dslink_socket_connect(Socket **sock,
                          const char *address,
                          unsigned short port,
                          uint_fast8_t secure);

int dslink_socket_read(Socket *sock, char *buf, size_t len);
int dslink_socket_read_timeout(Socket *sock, char *buf,
                               size_t len, uint32_t timeout);

int dslink_socket_write(Socket *sock, char *buf, size_t len);

void dslink_socket_close(Socket *sock);

#ifdef __cplusplus
}
#endif

#endif // SDK_DSLINK_C_SOCKET_H