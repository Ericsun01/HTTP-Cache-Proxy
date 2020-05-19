#ifndef SOCKET_H
#define SOCKET_H

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <exception>

using namespace std;

class Socket{
 public:
  int status;
  int fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname;
  const char *port;
  
 public:
  Socket(){
    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
  }
  
  void createNewSocket(const char *hostname, const char *port){
    this->hostname = hostname;
    this->port = port;
    
    status = getaddrinfo(hostname, port, &host_info, &host_info_list);
    if (status != 0) {
      throw std::exception();
      //return -1;
    } //if getaddrinfo fails
    
    fd = socket(host_info_list->ai_family, 
                host_info_list->ai_socktype, 
                host_info_list->ai_protocol);
    if (fd == -1) {
      throw std::exception();
      //return -1;
    } //If socket fails
  }

  int getFd(){
    return fd;
  }
  
  virtual ~Socket(){
    //freeaddrinfo(host_info_list);
    //close(fd); 
  }
};

#endif
