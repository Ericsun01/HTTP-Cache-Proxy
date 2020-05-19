#ifndef CONNECTSOCKET_H
#define CONNECTSOCKET_H

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "Socket.h"

using namespace std;

class ConnectSocket : public Socket {
  
 public:
  ConnectSocket(){
    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
  }
  
  void connectTarget(){
    status = connect(fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
      throw std::exception();
      //return -1;
    } //if connect fails
    //return 0;
  }

  int getFd(){
    return fd;
  }
};

#endif
