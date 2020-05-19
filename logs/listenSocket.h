#include "Socket.h"

class ListenSocket : public Socket { 
 private:
  int yes = 1;
  int client_fd;

 public:
  ListenSocket(){
    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
    host_info.ai_flags = AI_PASSIVE; 
  }

  //wait for other's connection
  void waitForConnection(){
    status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    status = bind(fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
      throw std::exception();
    } //if bind fails
    status = listen(fd, 100);
    if (status == -1) {
      throw std::exception();    
    } //if listen fails
  }


  //accept other player's connection, return corresponding fd
  int acceptConnection(){
    struct sockaddr_storage socket_addr;
    socklen_t socket_addr_len = sizeof(socket_addr);
    client_fd = accept(fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
    if (client_fd == -1) {
      throw std::exception();
    } //if accept fails                                               
    return client_fd;
  }
};
