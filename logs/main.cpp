#include <typeinfo>
#include <vector>
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include "listenSocket.h"
#include "ConnectSocket.h"
#include "Message.hpp"
#include "Cache.hpp"

#define RECV_LENGTH 2048
#define SEND_LENGTH 2048
#define HEADER_LENGTH 8192
#define DEBUG 0

std::ofstream logPrinter;
std::mutex uidMutex;
int uid = 0;

// strong guarantee
std::string getCurrTime() { 
  time_t currTime = time(0);
  tm *tm = localtime(&currTime);
  return std::string(asctime(tm));
}

int sendCachedMsg(Message msg, int recvFd, int sendFd){
  std::vector<std::string> body = msg.getBody();
  for(size_t i=0;i<body.size();i++){
    if(send(recvFd,body[i].c_str(),body[i].size(),0)==-1){
      close(recvFd);
      close(sendFd);
      return -1;
    }
  }
  return 0;
}

int recvMsg(int fd, vector<char> &buffer){
  int recvLen = 0;
  buffer = vector<char>(65536, '\0');
  if((recvLen = recv(fd, &buffer.data()[0], buffer.size(), 0)) <= 0){
    return recvLen;
  }
  buffer[recvLen] = '\0';
  return recvLen;
}

int sendAll(int sendFd, int recvFd, vector<char> &buffer, int recvLen){
  int sendLen;
  if((sendLen = send(recvFd,&buffer.data()[0],recvLen,0))==-1){
    close(recvFd);
    close(sendFd);
    return -1;
  };
  
  while(sendLen<recvLen){
    int tempLen;
    if((tempLen = send(recvFd,&buffer.data()[sendLen],recvLen-sendLen,0))==-1){
      close(recvFd);
      close(sendFd);
      return -1;
    };
    sendLen += tempLen;
  }
  return sendLen;
}

int handleLargeMsg(std::vector<char> &buffer, int recvLen, int client_fd, int server_fd, Message &response){
  int headerLen;
  int sendLen;
  int totalLen = recvLen;
  headerLen = response.getHeaderLen();
  if(response.getMapValue("Content-Length")!=""){
    int contentLen = stoi(response.getMapValue("Content-Length"));
    
    //recv until finished
    while(totalLen < contentLen+headerLen){
          
      buffer = std::vector<char>(65536, '\0');
      if((recvLen = recvMsg(server_fd, buffer))<=0){
        close(client_fd);
        close(server_fd);
        return -1;
      }
      //store received info in response
      response.addToBody(std::string(buffer.begin(),buffer.begin()+recvLen));
      totalLen += recvLen;
      //send message to client
      if((sendLen = sendAll(server_fd, client_fd ,buffer,recvLen))==-1){
        return -1;
      }
    }
  }
  else if(response.getMapValue("Transfer-Encoding")=="chunked"){
    
    buffer = std::vector<char>(65536, '\0');
    if((recvLen = recvMsg(server_fd, buffer))<=0){
      close(client_fd);
      close(server_fd);
      return -1;
    }
    if((sendLen = sendAll(server_fd, client_fd,buffer,recvLen))==-1){
      return -1;
    }
    
    while(buffer.data()[0]!='0'){
      buffer = std::vector<char>(65536, '\0');
      if((recvLen = recvMsg(server_fd, buffer))<=0){
        close(client_fd);
        close(server_fd);
        return -1;
      }

      //store received info in response
      response.addToBody(std::string(buffer.begin(),buffer.begin()+recvLen));
      if((sendLen = sendAll(server_fd, client_fd,buffer,recvLen))==-1){
        return -1;
      }
    }
  }
  close(client_fd);
  close(server_fd);
  return 0;
}

int doubleTransfer(int sendFd, int recvFd){
  int recvLen = 0;
  std::vector<char> buffer = std::vector<char>(65536, '\0');
  if ((recvLen = recv(sendFd, &buffer.data()[0], buffer.size(), 0)) <= 0) {
      // got error or connection closed by client
    close(sendFd);
    close(recvFd);
    return -1;
  }
  else{
    std::string response(buffer.begin(),buffer.begin()+recvLen);
    if(send(recvFd,response.c_str(),recvLen,0) == -1){
      close(sendFd);
      close(recvFd);
      return -1;
    }
  }
  return 0;
}

//IO multiplexing for CONNECT
void doublePort(int clientFd, int serverFd){
  fd_set fdSet;
  int fds[] = {clientFd,serverFd};
  int fdMax = std::max(fds[0],fds[1]);
  while(1){
    FD_ZERO(&fdSet);
    FD_SET(fds[0], &fdSet);
    FD_SET(fds[1], &fdSet);
    
    if(select(fdMax+1, &fdSet, NULL, NULL, NULL) == -1){
      return;
    }
    for(int i = 0; i < 2; i++){
      //request from client
      if(FD_ISSET(fds[i], &fdSet)){
        if(doubleTransfer(fds[i], fds[1-i]) == -1){
          break;
        }
      }
    }
  }
}

//main funcion to run proxy
void runProxy(int client_fd, LRUCache *cache) {
  
  int status = 0;
  int recvLen = 0;
  int sendLen = 0;
  std::vector<char> buffer = std::vector<char>(65536, '\0');
  
  if((recvLen = recvMsg(client_fd, buffer))<=0){
    if(recvLen==-1){
      std::cerr << uid << ": ERROR: lock unique id" << std::endl;
      logPrinter << uid << ": ERROR: lock unique id" << std::endl;
    }
    close(client_fd);
    return;
  }
;
  Request initRequest(std::string(buffer.begin(),buffer.begin()+recvLen));
  initRequest.parseHeader();

  try{
    std::lock_guard<std::mutex> mutex(uidMutex);
    initRequest.setUid(uid);
    uid++;
  } catch(std::exception &e) {
    std::cerr << uid << ": ERROR: lock unique id" << std::endl;
    logPrinter << uid << ": ERROR: lock unique id" << std::endl;
    close(client_fd);
    return;
  }

  try{
    std::cout << initRequest.getUid() << ": " << initRequest.getHeadLine() << " from " << initRequest.getIp() << " @ " << cache->getCurrTime() << std::endl;
    logPrinter << initRequest.getUid() << ": " << initRequest.getHeadLine() << " from " << initRequest.getIp() << " @ " << cache->getCurrTime() << std::endl;
  }catch(std::exception &e){
    std::cerr << initRequest.getUid() << ": ERROR: print new request" << std::endl;
    logPrinter << initRequest.getUid() << ": ERROR: print new request" << std::endl;
    close(client_fd);
  }
      
  ConnectSocket cs;
  try{
    cs.createNewSocket(initRequest.getIp().c_str(),initRequest.getPort().c_str());
    cs.connectTarget();
  }catch(std::exception &e){
    std::cerr << initRequest.getUid() << ": ERROR: create ConnectSocket" << std::endl;
    logPrinter << initRequest.getUid() << ": ERROR: create ConnectSocket" << std::endl;
    close(client_fd);
    return;
  }
  
  std::string method = initRequest.getMethod();
  
  if (method == "GET") {
    std::string url = initRequest.getUrl(); 

    //handle cache
    //if url is not in cache
    if(!cache->ifInCache(url)){
      std::cout << initRequest.getUid() << ": not in cache" << std::endl;
      logPrinter << initRequest.getUid() << ": not in cache" << std::endl;
      // send message to server
      std::cout << initRequest.getUid() << ": Requesting " << initRequest.getHeadLine() << " from" << initRequest.getIp() << std::endl;
      logPrinter << initRequest.getUid() << ": Requesting " << initRequest.getHeadLine() << " from" << initRequest.getIp() << std::endl;
      if((sendLen = sendAll(client_fd, cs.fd,buffer,recvLen))==-1){
        std::cerr << initRequest.getUid() << ": ERROR: send to client in GET not in cache" << std::endl;
        logPrinter << initRequest.getUid() << ": ERROR: send to server in GET not in cache" << std::endl;
        return;
      }
      if((recvLen = recvMsg(cs.fd, buffer))<=0){
        if(recvLen==-1){
          std::cerr << initRequest.getUid() << ": ERROR: recv from server in GET not in cache" << std::endl;
          logPrinter << initRequest.getUid() << ": ERROR: recv from server in GET not in cache" << std::endl;
        }
        close(client_fd);
        close(cs.fd);
        return;
      }
      if((sendLen = sendAll(cs.fd, client_fd,buffer,recvLen))==-1){
        std::cerr << initRequest.getUid() << ": ERROR: send to client in GET not in cache" << std::endl;
        logPrinter << initRequest.getUid() << ": ERROR: send to client in GET" << std::endl;
        return;
      }
      Message initResponse(std::string(buffer.begin(),buffer.begin()+recvLen));
      initResponse.parseHeader();
      if((handleLargeMsg(buffer, recvLen, client_fd, cs.fd, initResponse))==-1){
        std::cerr << initRequest.getUid() << ": ERROR: handle large message in GET not in cache" << std::endl;
        logPrinter << initRequest.getUid() << ": ERROR: handle large message in GET not in cache" << std::endl;
        return;
      }
      
      if(initRequest.getIfNoStore()!=true&&initResponse.getIfNoStore()!=true){
        cache->add(url,initResponse);
      }
    }
    
    //if url is in cache
    else{
      //cout << "[CACHE] find in cache" << endl;
      Message cachedResponse = cache->get(url);
      if(initRequest.getIfNoCache()==true || cachedResponse.getIfNoCache()==true||!cache->checkIfValid(initRequest)){
        //need revalidate
        if(initRequest.getIfNoCache()==false && cachedResponse.getIfNoCache()==false &&
           !cache->checkIfValid(initRequest)){
          std::cout << initRequest.getUid() << ": in cache, but expired at" << std::endl;
          logPrinter << initRequest.getUid() << ": in cache, but expired at" << std::endl;
        }
        else{
          std::cout << initRequest.getUid() << ": in cache, requires validation" << std::endl;
          logPrinter << initRequest.getUid() << ": in cache, requires validation" << std::endl;
        }
        cache->getValidateReq(initRequest,cachedResponse);
        if(sendCachedMsg(initRequest,cs.fd,client_fd)==-1){
          std::cerr << initRequest.getUid() <<  ": ERROR: send validation in GET find in cache" << std::endl;
          logPrinter << initRequest.getUid() <<  ": ERROR: send validation in GET find in cache" << std::endl;
          return;
        }
        // recv first message from server
        if((recvLen = recvMsg(cs.fd, buffer))<=0){
          if(recvLen==-1){
            std::cerr << initRequest.getUid() <<  ": ERROR: recv from server in GET find in cache" << std::endl;
            logPrinter << initRequest.getUid() <<  ": ERROR: recv from server in GET find in cache" << std::endl;
          }
          close(client_fd);
          close(cs.fd);
          return;
        }
        
        Message newResponse(std::string(buffer.begin(),buffer.begin()+recvLen));
        newResponse.parseHeader();
        std::cout << initRequest.getUid() << ": Received " << newResponse.getHeadLine() << " from" << initRequest.getIp() << std::endl;
        logPrinter << initRequest.getUid() << ": Received " << newResponse.getHeadLine() << " from" << initRequest.getIp() << std::endl;
        if(newResponse.getHeadLine().find("304")!=std::string::npos){
          //cout << "[CACHE] 304 valid" << endl;
          if(sendCachedMsg(cachedResponse,client_fd,cs.fd)==-1){
            std::cerr << initRequest.getUid() <<  ": ERROR: 304 send to client in GET find in cache" << std::endl;
            logPrinter << initRequest.getUid() <<  ": ERROR: 304 send to client in GET find in cache" << std::endl;
            return;
          }
        }
        else{
          //send message to client
          if((sendLen = sendAll(cs.fd, client_fd,buffer,recvLen))==-1){
            std::cerr << initRequest.getUid() <<  ": ERROR: send new response to client in GET find in cache" << std::endl;
            logPrinter << initRequest.getUid() <<  ": ERROR: send new response to client in GET find in cache" << std::endl;
            return;
          }
          
          if((handleLargeMsg(buffer, recvLen, client_fd, cs.fd, newResponse))==-1){
            std::cerr << initRequest.getUid() <<  ": ERROR: handle large message in GET find in cache" << std::endl;
            logPrinter << initRequest.getUid() <<  ": ERROR: handle large message in GET find in cache" << std::endl;
            return;
          }

          if(initRequest.getIfNoStore()!=true && newResponse.getIfNoStore()!=true &&
             newResponse.getHeadLine().find("200")!=std::string::npos){
            cache->add(url, newResponse);
          }
        }
        
      }
      else{
        //no need revalidate
        std::cout << initRequest.getUid() << ": in cache, valid" << std::endl;
        logPrinter << initRequest.getUid() << ": in cache, valid" << std::endl;
        if(sendCachedMsg(cachedResponse,client_fd,cs.fd)==-1){
          std::cerr << initRequest.getUid() <<  ": ERROR: send valid msg to client in GET find in cache" << std::endl;
          logPrinter << initRequest.getUid() <<  ": ERROR: send valid msg to client in GET find in cache" << std::endl;
          return;
        }
        std::cout << initRequest.getUid() << ": Responding " << cachedResponse.getHeadLine() << std::endl;
        logPrinter << initRequest.getUid() << ": Responding " << cachedResponse.getHeadLine() << std::endl;
      }
    }
    close(client_fd);
    close(cs.fd);
  }
  
  else if(method == "POST"){
    // send message to server
    std::cout << initRequest.getUid() << ": Requesting " << initRequest.getHeadLine() << " from" << initRequest.getIp() << std::endl;
    logPrinter << initRequest.getUid() << ": Requesting " << initRequest.getHeadLine() << " from" << initRequest.getIp() << std::endl;

    if((sendLen = sendAll(client_fd, cs.fd,buffer,recvLen))==-1){
      std::cerr << initRequest.getUid() <<  ": ERROR: send to server in POST" << std::endl;
      logPrinter << initRequest.getUid() <<  ": ERROR: send to server in POST" << std::endl;
      return;
    };
    
    // recv first message from server
    buffer = std::vector<char>(65536, '\0');
    if((recvLen = recvMsg(cs.fd, buffer))<=0){
      if(recvLen==-1){
        std::cerr << initRequest.getUid() <<  ": ERROR: recv from server in POST" << std::endl;
        logPrinter << initRequest.getUid() <<  ": ERROR: recv from server in POST" << std::endl;
      }
      close(client_fd);
      close(cs.fd);
      return;
    }
    
    Message initResponse(std::string(buffer.begin(),buffer.begin()+recvLen));
    initResponse.parseHeader();
    //send message to client
    std::cout << initRequest.getUid() << ": Received " << initResponse.getHeadLine() << " from" << initRequest.getIp() << std::endl;
    logPrinter << initRequest.getUid() << ": Received " << initResponse.getHeadLine() << " from" << initRequest.getIp() << std::endl;
    if((sendLen = sendAll(cs.fd, client_fd,buffer,recvLen))==-1){
      std::cerr << initRequest.getUid() <<  ": ERROR: send to client in POST" << std::endl;
      logPrinter << initRequest.getUid() <<  ": ERROR: send to client in POST" << std::endl;
      return;
    }
    
    if((handleLargeMsg(buffer, recvLen, client_fd, cs.fd, initResponse))==-1){
      std::cerr << initRequest.getUid() <<  ": ERROR: handle large message in POST" << std::endl;
      logPrinter << initRequest.getUid() <<  ": ERROR: handle large message in POST" << std::endl;
      return;
    }
    
  }
  
  else if (method == "CONNECT") {
    std::string message = "HTTP/1.1 200 OK\r\n\r\n";
    status = send(client_fd, message.c_str(), message.size(), 0);
    if (status == -1) {
      std::cerr << initRequest.getUid() <<  ": ERROR: send 200 OK in CONNECT" << std::endl;
      logPrinter << initRequest.getUid() <<  ": ERROR: send 200 OK in CONNECT" << std::endl;
      return;
    }
    while (1) {
      fd_set sockset;
      FD_ZERO(&sockset);
      int maxfd = client_fd > cs.fd ? client_fd : cs.fd;
      FD_SET(client_fd, &sockset);
      FD_SET(cs.fd, &sockset);
      int status = select(maxfd + 1, &sockset, nullptr, nullptr, nullptr);
      if (status == -1) {
        std::cerr << initRequest.getUid() <<  ": ERROR: select in CONNECT" << std::endl;
        logPrinter << initRequest.getUid() <<  ": ERROR: select in CONNECT" << std::endl;
        break;
      }
      if (FD_ISSET(client_fd, &sockset)) {
        try {
          doublePort(client_fd, cs.fd);
        } catch (std::exception &e) {
          std::cerr << initRequest.getUid() <<  ": Tunnel closed" << std::endl;
          logPrinter << initRequest.getUid() <<  ": Tunnel closed" << std::endl;
          break;
        }
        
      } else {
        try {
          doublePort(cs.fd, client_fd);
        } catch (std::exception &e) {
          std::cerr << initRequest.getUid() <<  ": Tunnel closed" << std::endl;
          logPrinter << initRequest.getUid() <<  ": Tunnel closed" << std::endl;
          break;
        } 
      }
    } 
    close(client_fd);
    close(cs.fd);
  } 
}

int main(int argc, char **argv) {

  try {
    logPrinter.open("../proxy.log", std::ostream::out);
  } catch (std::exception &e) {
    std::cerr << "0: ERROR: initialize log printer" << std::endl;
    return EXIT_FAILURE;
  }
  
  ListenSocket ls; 
  try {
    ls.createNewSocket(NULL,"12345");
    ls.waitForConnection();
  } catch (std::exception &e) {
    std::cerr << "0: ERROR: create ListenSocket"<< std::endl;
    logPrinter << "0: ERROR: create ListenSocket"<< std::endl;
    return EXIT_FAILURE;
  }

  LRUCache cache(100);
  while (1) {
    int client_fd;
    try{
      client_fd = ls.acceptConnection();
    }catch(std::exception &e){
      std::cerr << "0: ERROR: accept connection failed" << std::endl;
      logPrinter << "0: ERROR: accept connection failed" << std::endl;
      continue;
    }
    if (client_fd > 0) {
      try {
        std::thread proxy(runProxy, client_fd, &cache);
        proxy.detach();

      } catch (std::exception &e) {
        std::cerr << "0: ERROR: connection failed(" << client_fd << ")"<< std::endl;
        logPrinter << "0: ERROR: connection failed(" << client_fd << ")"<< std::endl;
      }
    }
  }
  return EXIT_SUCCESS;
}
