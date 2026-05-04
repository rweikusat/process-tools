/*
  initialize a sockaddr_un structure
*/

/*  includes */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "diag.h"
#include "fill_sun.h"

/*  routines */
void fill_sun(char *addr,
              struct sockaddr_un *sun, unsigned *sun_len)
{
    size_t a_len;
    char *a_dst;

    sun->sun_family = AF_UNIX;
    a_len = strlen(addr);
    a_dst = sun->sun_path;

    if (*addr == '/' && addr[1] == '/') {
        a_len -= 2;
        addr += 2;

        *a_dst++ = 0;
    } else
        ++a_len;

    if (a_len > (sun->sun_path + sizeof(sun->sun_path)) - a_dst) {
        err("%s: addr too large", __func__);
        exit(1);
    }

    memcpy(a_dst, addr, a_len);
    *sun_len = offsetof(struct sockaddr_un, sun_path) + a_len;
}
