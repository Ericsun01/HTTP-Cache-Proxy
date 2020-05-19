#include <unistd.h>
#include <string>
#include <iostream>
#include <vector>
#include "Message.hpp"

class Request: public Message{
private:
  std::string method;
  std::string ip;
  std::string port;
  
public:
  Request(std::string s1, std::string s2, std::string s3):method(s1),ip(s2),port(s3){};

  std::string getMethod(){
    return this->method;
  }
  std::string getIp(){
    return this->ip;
  }
  std::string getPort(){
    return this->port;
  }  
};
