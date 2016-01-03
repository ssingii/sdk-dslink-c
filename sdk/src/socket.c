#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net.h>

#include "dslink/socket.h"
#include "dslink/utils.h"
#include "dslink/err.h"

struct SslSocket {

    uint_fast8_t secure;
    mbedtls_net_context *socket_fd;
    mbedtls_entropy_context *entropy;
    mbedtls_ctr_drbg_context *drbg;
    mbedtls_ssl_context *ssl;
    mbedtls_ssl_config *conf;

};

struct Socket {

    uint_fast8_t secure;
    mbedtls_net_context *socket_fd;

};

static
Socket *dslink_socket_init(uint_fast8_t secure) {
    if (secure) {
        SslSocket *s = malloc(sizeof(SslSocket));
        if (!s) {
            return NULL;
        }
        s->secure = 1;
        s->socket_fd = malloc(sizeof(mbedtls_net_context));
        s->entropy = malloc(sizeof(mbedtls_entropy_context));
        s->drbg = malloc(sizeof(mbedtls_ctr_drbg_context));
        s->ssl = malloc(sizeof(mbedtls_ssl_context));
        s->conf = malloc(sizeof(mbedtls_ssl_config));

        if (!(s->socket_fd && s->entropy && s->drbg && s->ssl && s->conf)) {
            DSLINK_CHECKED_EXEC(free, s->socket_fd);
            DSLINK_CHECKED_EXEC(free, s->entropy);
            DSLINK_CHECKED_EXEC(free, s->drbg);
            DSLINK_CHECKED_EXEC(free, s->ssl);
            DSLINK_CHECKED_EXEC(free, s->conf);
            free(s);
            return NULL;
        }

        mbedtls_net_init(s->socket_fd);
        mbedtls_entropy_init(s->entropy);
        mbedtls_ctr_drbg_init(s->drbg);
        mbedtls_ssl_init(s->ssl);
        mbedtls_ssl_config_init(s->conf);
        return (Socket *) s;
    } else {
        Socket *s = malloc(sizeof(Socket));
        if (!s) {
            return NULL;
        }
        s->secure = 0;
        s->socket_fd = malloc(sizeof(mbedtls_net_context));
        if (!s->socket_fd) {
            free(s);
            return NULL;
        }
        return s;
    }
}

static
int dslink_socket_connect_secure(SslSocket *sock,
                                 const char *address,
                                 unsigned short port) {
    if ((errno = mbedtls_ctr_drbg_seed(sock->drbg, mbedtls_entropy_func,
                                       sock->entropy, NULL, 0)) != 0) {
        return DSLINK_CRYPT_ENTROPY_SEED_ERR;
    }

    char num[6];
    snprintf(num, sizeof(num), "%d", port);
    if ((errno = mbedtls_net_connect(sock->socket_fd, address,
                                     num, MBEDTLS_NET_PROTO_TCP)) != 0) {
        return DSLINK_SOCK_CONNECT_ERR;
    }

    if ((errno = mbedtls_ssl_config_defaults(sock->conf,
                                             MBEDTLS_SSL_IS_CLIENT,
                                             MBEDTLS_SSL_TRANSPORT_STREAM,
                                             MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        return DSLINK_SOCK_SSL_CONFIG_ERR;
    }
    mbedtls_ssl_conf_authmode(sock->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(sock->conf, mbedtls_ctr_drbg_random, sock->drbg);

    if ((errno = mbedtls_ssl_setup(sock->ssl, sock->conf)) != 0) {
        return DSLINK_SOCK_SSL_SETUP_ERR;
    }

    if ((errno = mbedtls_ssl_set_hostname(sock->ssl, "_")) != 0) {
        return DSLINK_SOCK_SSL_HOSTNAME_SET_ERR;
    }

    mbedtls_ssl_set_bio(sock->ssl, sock->socket_fd,
                        mbedtls_net_send, mbedtls_net_recv, NULL);

    int stat;
    while ((stat = mbedtls_ssl_handshake(sock->ssl)) != 0) {
        if (stat != MBEDTLS_ERR_SSL_WANT_READ
            && stat != MBEDTLS_ERR_SSL_WANT_WRITE) {
            errno = stat;
            return DSLINK_SOCK_SSL_HANDSHAKE_ERR;
        }
    }

    return 0;
}

static
int dslink_socket_connect_insecure(Socket *sock,
                                   const char *address,
                                   unsigned short port) {
    mbedtls_net_init(sock->socket_fd);
    char num[6];
    snprintf(num, sizeof(num), "%d", port);
    if ((errno = mbedtls_net_connect(sock->socket_fd, address,
                                     num, MBEDTLS_NET_PROTO_TCP)) != 0) {
        return DSLINK_SOCK_CONNECT_ERR;
    }
    return 0;
}

int dslink_socket_connect(Socket **sock,
                          const char *address,
                          unsigned short port,
                          uint_fast8_t secure) {
    *sock = dslink_socket_init(secure);
    if (!(*sock)) {
        return DSLINK_ALLOC_ERR;
    }
    if (secure) {
        SslSocket *s = (SslSocket *) *sock;
        return dslink_socket_connect_secure(s, address, port);
    } else {
        return dslink_socket_connect_insecure(*sock, address, port);
    }
}

int dslink_socket_read(Socket *sock, char *buf, size_t len) {
    int r;
    if (sock->secure) {
        r = mbedtls_ssl_read(((SslSocket *) sock)->ssl,
                             (unsigned char *) buf, len);
    } else {
        r = mbedtls_net_recv(sock->socket_fd, (unsigned char *) buf, len);
    }
    if (r < 0) {
        errno = r;
        return DSLINK_SOCK_READ_ERR;
    }
    return r;
}

int dslink_socket_read_timeout(Socket *sock, char *buf,
                               size_t len, uint32_t timeout) {
    int r;
    if (sock->secure) {
        SslSocket *s = (SslSocket *) sock;
        mbedtls_ssl_conf_read_timeout(s->conf, timeout);
        r = mbedtls_ssl_read(s->ssl, (unsigned char *) buf, len);
    } else {
        r = mbedtls_net_recv_timeout(sock->socket_fd,
                                     (unsigned char *) buf, len, timeout);
    }
    if (r < 0) {
        errno = r;
        return DSLINK_SOCK_READ_ERR;
    }
    return r;
}

int dslink_socket_write(Socket *sock, char *buf, size_t len) {
    int r;
    if (sock->secure) {
        r = mbedtls_ssl_write(((SslSocket *) sock)->ssl,
                              (unsigned char *) buf, len);
    } else {
        r = mbedtls_net_send(sock->socket_fd, (unsigned char *) buf, len);
    }
    if (r < 0) {
        errno = r;
        return DSLINK_SOCK_WRITE_ERR;
    }
    return r;
}

void dslink_socket_close(Socket *sock) {
    if (!sock) {
        return;
    }
    if (sock->secure) {
        SslSocket *s = (SslSocket *) sock;
        mbedtls_ssl_close_notify(s->ssl);
        mbedtls_entropy_free(s->entropy);
        mbedtls_ctr_drbg_free(s->drbg);
        mbedtls_ssl_free(s->ssl);
        mbedtls_ssl_config_free(s->conf);
        free(s->entropy);
        free(s->drbg);
        free(s->ssl);
        free(s->conf);
    }
    mbedtls_net_free(sock->socket_fd);
    free(sock->socket_fd);
    free(sock);
}