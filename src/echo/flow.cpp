#include "flow.h"
#include "server.h"


template <class Iterator>
std::string toHex(Iterator _it, Iterator _end, std::string _prefix)
{
  typedef std::iterator_traits<Iterator> traits;
  static_assert(sizeof(typename traits::value_type) == 1, "toHex needs byte-sized element type");

  static char const* hexdigits = "0123456789abcdef";
  size_t off = _prefix.size();
  std::string hex(std::distance(_it, _end)*2 + off, '0');
  hex.replace(0, off, _prefix);
  for (; _it != _end; _it++)
  {
    hex[off++] = hexdigits[(*_it >> 4) & 0x0f];
    hex[off++] = hexdigits[*_it & 0x0f];
  }
  return hex;
}

/// Convert a series of bytes to the corresponding hex string.
/// @example toHex("A\x69") == "4169"
 std::string toHex(std::string const& _data)
{
  return toHex(_data.begin(), _data.end(), "");
}

/// Convert a series of bytes to the corresponding hex string with 0x prefix.
/// @example toHexPrefixed("A\x69") == "0x4169"
template <class T> std::string toHexPrefixed(T const& _data)
{
  return toHex(_data.begin(), _data.end(), "0x");
}

FlowBase::~FlowBase() {
  if (coro_) {
    destroy_coro(coro_);
    coro_ = nullptr;
  }
}

void FlowBase::Init(struct ev_loop * loop, int fd, const std::string& data) {
  coro_ = create_coro(nullptr, nullptr);
  assert(coro_);
  loop_ = loop;
  fd_ = fd;
  client_data = data;
}

void FlowBase::Entry() {
  printf ("Begin handle: %s\n", toHex(client_data).c_str());
}

int FlowBase::SendTo(int fd,
                     const std::string& ip,
                     uint16_t port,
                     const std::string& send_data) {
  struct sockaddr_in     servaddr;
  FillSocketAddress(servaddr, ip, port);
  return (int)sendto(fd, send_data.c_str(), (int)send_data.length(),
                0, (const struct sockaddr *) &servaddr,
                (int)(sizeof(servaddr)));
}

int FlowBase::RecvFrom(int fd, std::string& recv_data) {
  char recv_buf[1024];
  struct sockaddr_in their_addr;
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  while (true) {
    socklen_t addr_len = sizeof(their_addr);
    int ret = (int)::recvfrom(fd, recv_buf, 1024, 0, (struct sockaddr *)&their_addr, &addr_len);
    if (errno == EINTR) continue;
    if (ret <= 0 && (errno == EAGAIN ||
                     errno == EWOULDBLOCK )) {

      WatchIORead(fd);
      yield_coro();
      UnWatchIORead(fd);
      continue;
    }
    if (ret <= 0 ) {
      printf ("some thing error happend, errno: %d\n", errno);
      return -1;
    }
    recv_data = std::string(recv_buf, ret);
    return 0;
  }
}

void FlowBase::WatchIORead(int fd) {
  fd_io_[fd].set(loop_);
  fd_io_[fd].set(ev::READ);
  fd_io_[fd].set<FlowBase, &FlowBase::CbRead>(this);
  fd_io_[fd].start(fd, ev::READ);
}

void FlowBase::UnWatchIORead(int fd) {
  assert(fd_io_.find(fd) != fd_io_.end());
  fd_io_[fd].stop();
}

int FlowBase::GenFd() {
  return socket(AF_INET, SOCK_DGRAM, 0);
}

void FlowBase::DestroyFd(int fd) {
  close (fd);
  auto it = fd_io_.find(fd);
  if (it != fd_io_.end()) {
    it->second.stop();
    fd_io_.erase(it);
  }
}

void FlowBase::CbRead(ev::io &watcher, int revents) {
  (void) watcher;
  (void) revents;
  // int fd = watcher.fd;
  resume_coro(coro_);
}

int FlowBase::SendBack(const std::string& data) {
  return (int)write (fd_, data.c_str(), (int)data.length());
}
