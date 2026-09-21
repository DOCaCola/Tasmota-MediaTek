#pragma once
#include <stddef.h>
#include <stdint.h>
typedef unsigned socklen_t;
struct sockaddr { unsigned short family; };
struct sockaddr_in { unsigned short sin_family, sin_port; struct { uint32_t s_addr; } sin_addr; };
#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define FIONBIO 3
#define MSG_DONTWAIT 4
int lwip_ioctl(int, int, void*);
int lwip_socket(int,int,int);
int lwip_setsockopt(int,int,int,const void*,socklen_t);
int lwip_bind(int,const struct sockaddr*,socklen_t);
int lwip_listen(int,int);
int lwip_accept(int,struct sockaddr*,socklen_t*);
int lwip_send(int,const void*,size_t,int);
int lwip_recv(int,void*,size_t,int);
int lwip_close(int);
uint16_t lwip_htons(uint16_t);
