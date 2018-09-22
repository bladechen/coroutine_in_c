#pragma once

#include <string>
#include <map>
#include "ev++.h"
#include "coro.h"

class FlowBase {
  friend class CServer;

  public:
    FlowBase() = default;
    void Init(struct ev_loop * loop, int fd, const std::string& data);
    virtual ~FlowBase();
    virtual void Entry() = 0;
    static void Entry(void* argv) {
      ((FlowBase*) argv)->Entry();
    }
    void CbRead(ev::io &watcher, int revents);

    // only for udp demo
    int GenFd();
    void DestroyFd(int fd);

    int SendTo(int fd, const std::string& ip,
               uint16_t port, const std::string& send_data);
    int RecvFrom(int fd, std::string& recv_data);

    int SendBack(const std::string& data);



  protected:
    void WatchIORead(int fd);
    void UnWatchIORead(int fd);

    struct ev_loop * loop_{nullptr};
    std::map<int, ev::io> fd_io_;
    std::string from_ip;
    uint16_t from_port;
    int fd_;
    std::string client_data;
    struct coroutine* coro_{nullptr};
};
