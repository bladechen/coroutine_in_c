#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <map>
#include <vector>
#include "ev++.h"
#include "echo.h"
#include "flow.h"

#include "coro.h"

typedef in_addr_t Host;
typedef in_port_t Port;

inline Port ConvertPort(uint16_t port) {
  return htons(port);
}

inline Host ConvertHost(const std::string &host) {
  struct in_addr addr;
  inet_aton(host.c_str(), &addr);
  return addr.s_addr;
}

inline void FillSocketAddress(sockaddr_in &addr, const std::string& host, uint16_t port) {
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = ConvertPort(port);
  addr.sin_addr.s_addr = ConvertHost(host);
}


inline bool GetPeerAddr(int fd, uint32_t& ip, uint16_t& port) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (getpeername(fd, (struct sockaddr*)&addr, &len) != 0) {
    printf ("getpeername error\n");
    return false;
  }

  // if (addr.ss_family == PF_INET) {
  struct sockaddr_in *s = (struct sockaddr_in *)&addr;
  port = ntohs(s->sin_port);
  ::memcpy(&ip, &s->sin_addr, sizeof(s->sin_addr));
  // char ipstr[64];
  // printf ("%s\n", inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof (ipstr)));
  return true;
}

inline bool GetPeerAddr(int fd, std::string& ip, uint16_t& port) {
  uint32_t ip_uint;
  if (GetPeerAddr(fd, ip_uint, port) == false) {
    return false;
  }

  char ipstr[64];
  if (inet_ntop(AF_INET, (const void *)(&ip_uint), ipstr, sizeof (ipstr)) == NULL) {
    return false;
  }

  ip = std::string(ipstr);
  return true;
}

class CServer {
  public:
    void Init (const std::string& ip, uint16_t port);

    void Run();

  private:
    void Listen();
    void Setup();
    void CbAccept(ev::io &watcher, int revents);
    void CbTimeout(ev::timer &timer, int revents);
    void CbRead(ev::io &watcher, int revents);

    void Clear(int);

    struct ev_loop * loop_{nullptr};
    ev::io ev_io_;
    ev::timer ev_timer_;
    int sock_{-1};

    std::string host_;
    uint16_t port_;

    std::map<int, ev::io> sessions_;
    std::map<int, std::vector<FlowBase*>> flows_;
};


