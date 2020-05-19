#include <unistd.h>
#include <string>
#include <iostream>
#include "Message.hpp"

int main(){
  Request r("HTTP/1.1 304 Not Modified/r/nDate: Thu, 27 Feb 2020 05:33:02 GMT/r/nServer: Apache/r/nConnection: close/r/n/r/n");
  std::cout << "Date = " << r.getMapValue("Date") << std::endl;
  std::cout << "Connection = " << r.getMapValue("Connection") << std::endl;
  
  
}
