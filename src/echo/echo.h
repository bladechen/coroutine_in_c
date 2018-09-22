#pragma once
#include <cassert>
#include "flow.h"

extern std::string toHex(const std::string&);
class CEcho : public FlowBase {
  public:
  private:
    void Entry() override{
      printf ("%d, get data [%s]\n", (int)time(NULL), toHex(client_data).c_str());

      std::string ack = client_data;
      int fd = GenFd();
      assert(fd >=0);
      assert((int)client_data.length() == SendTo(fd, "127.0.0.1", 12345, client_data));
      assert(RecvFrom(fd, ack) >= 0 && ack.length() == client_data.length());
      SendBack(ack);
    }
};
