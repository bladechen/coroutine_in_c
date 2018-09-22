#include "server.h"
void CServer::Init(const std::string& ip, uint16_t port) {
  bootstrap_coro_env();
  loop_ = ev_loop_new(0);
  host_ = ip;
  port_ = port;
  Listen();
  Setup();
}

void CServer::Listen() {
  int sock = -1;
  int r = -1;
  do {
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock <= 0) {
      printf("Failed to create socket\n");
      break;
    }

    int val = 1;
    r = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
    if (r != 0) {
      printf("Failed to call setsockopt\n");
      break;
    }

    r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (r != 0) {
      printf("Failed to set SO_REUSEADDR\n");
      break;
    }

    struct sockaddr_in addr;
    FillSocketAddress(addr, host_, port_);
    r = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (r != 0) {
      printf("Failed to call bind\n");
      break;
    }

    r = listen(sock, 5);
    if (r != 0) {
      printf("Failed to call listen\n");
      break;
    }

    r = fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    if (r != 0) {
      printf("fcntl error\n");
      break;
    }

    sock_ = sock;
    printf("Listen: endpoint = %s:%u\n", inet_ntoa(addr.sin_addr),
                ntohs(addr.sin_port));
    return;
  } while (0);

  if (r != 0) {
    if (sock >= 0) {
      close(sock);
      sock = sock_ = -1;
      abort();
    }
  }
}

void CServer::Setup() {
  ev_io_.set(loop_);
  ev_io_.set<CServer, &CServer::CbAccept>(this);
  ev_io_.start(sock_, ev::READ);


  ev_timer_.set(loop_);
  ev_timer_.set<CServer, &CServer::CbTimeout>(this);
  ev_timer_.repeat = 0.001;
  ev_timer_.again();
}

void CServer::CbTimeout(ev::timer &, int ) {
  schedule_loop();
}

void CServer::CbAccept(ev::io &watcher, int revents) {
  if (ev::ERROR & revents) {
    printf("Wrong events\n");
    return;
  }

  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  int sock = accept(watcher.fd, (struct sockaddr *)&addr, &len);

  if (sock < 0) {
    printf("Failed to accept\n");
    return;
  }

  assert(!fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK)); // nonblock

  std::string ip;
  uint16_t port;
  assert(GetPeerAddr(sock, ip, port));
  printf ("accept connect: [%s:%d]\n", ip.c_str(), (int)port);

  assert(sock >= 0);

  assert(sessions_.find(sock) == sessions_.end());
  sessions_[sock].set(loop_);
  sessions_[sock].set(ev::READ);
  sessions_[sock].set<CServer, &CServer::CbRead>(this);
  sessions_[sock].start(sock, ev::READ);
}


void CServer::CbRead(ev::io &watcher, int revents) {
  (void) revents;
  int fd = watcher.fd;
  char* buf = new char[1024];
  int ret = (int)::recv(fd, buf, 1024, 0);
  if (ret <= 0 ) {
    printf ("close remote connection\n");
    Clear(fd);
    delete buf;
    return;
  } else {
    FlowBase* flow = new CEcho();
    flow->Init(loop_, fd, std::string{buf, (size_t)ret});
    flows_[fd].push_back(flow);
    restart_coro(flow->coro_, FlowBase::Entry, flow);
    delete buf;
  }
}

void CServer::Run() {
  ev_run(loop_, 0);
}

void CServer::Clear(int fd) {
  auto it = sessions_.find(fd);
  if (it != sessions_.end()) {
    it->second.stop();
    sessions_.erase(it);
  }
  auto it_flow = flows_.find(fd);
  if (it_flow != flows_.end()) {
    for (auto& flow : it_flow->second) {
      delete flow;
    }
    flows_.erase(it_flow);
  }
  close(fd);
}
