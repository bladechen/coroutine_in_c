#include "server.h"

int main() {
  CServer svr;
  svr.Init("127.0.0.1", 54321);
  svr.Run();
}
