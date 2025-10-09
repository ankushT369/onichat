#ifndef TCONNECT_H
#define TCONNECT_H


#define TOR_SOCKS_IP "127.0.0.1"
#define TOR_SOCKS_PORT 9050


int connect_via_tor(const char* onion_addr, int dest_port);

#endif // TCONNECT_H
