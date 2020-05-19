#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <unistd.h>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

class Message{

protected:
  std::string headLine;//end with /r/n
  std::string header;
  std::unordered_map<std::string, std::string> headerMap; //end with /r/n/r/n
  std::vector<std::string> body;//the rest
  bool ifNoCache;
  bool ifNoStore;
  int maxAge;
  int maxStale;
  int minFresh;
  //time_t cacheTime;
  
public:

  //constructor to parse headLine, header and body
  explicit Message(std::string httpMessage){
    if(httpMessage==""){
      std::cerr << "Error: HTTP message is empty" << std::endl;
      //throw std::exception();
    }
    else if(httpMessage.find("\r\n")==std::string::npos){
      std::cerr << "Error: HTTP message is empty" << std::endl;
      //throw std::exception();
    }
    else if(httpMessage.find("\r\n\r\n")==std::string::npos){
      std::cerr << "Error: HTTP message is empty" << std::endl;
      //throw std::exception();
    }
    else{
      this->headLine = httpMessage.substr(0,httpMessage.find("\r\n"));
      this->header = httpMessage.substr(0, httpMessage.find("\r\n\r\n")+4);
      this->body.push_back(httpMessage);
    }
  } 

  void insertToHeader(std::string s){
    std::string A = s.substr(0,s.find_first_of(":"));
    std::string B = s.substr(s.find_first_of(" ")+1);
    headerMap.insert(std::pair<std::string,std::string>(A,B));
    s = s.insert(0,"\r\n");
    this->header = this->header.insert(this->header.find("\r\n\r\n"),s);
    this->body[0] = this->body[0].insert(this->body[0].find("\r\n\r\n"),s);
  }

  std::string getStringFormat(){
    std::string ans;
    for(size_t i=0; i<body.size();i++){
      ans += body[i];
    }
    return ans;
  }
  
  
  void addToBody(std::string message){
    this->body.push_back(message);
  }

  /*void setCacheTime(time_t t){
    this->cacheTime = t;
  }

  time_t getCacheTime(){
    return this->cacheTime;
    }*/
  
  bool getIfNoCache(){
    return this->ifNoCache;
  }
  bool getIfNoStore(){
    return this->ifNoStore;
  }
  int getMaxAge(){
    return this->maxAge;
  }
  int getMaxStale(){
    return this->maxStale;
  }
  int getMinFresh(){
    return this->minFresh;
  }
  
  
  int getHeaderLen(){
    return this->header.size();
  }

  std::string getHeadLine(){
    return this->headLine;
  }
  
  std::vector<std::string> getBody(){
    return this->body;
  }
  
  //parse kvpair of a line in header
  void parsePair(std::string pairLine){
    if(pairLine.find(":")==std::string::npos){
      std::cerr << "Error: invalid pairLine" << std::endl;
      //throw std::exception();
    }
    std::string key = pairLine.substr(0,pairLine.find(":"));
    std::string value = pairLine.substr(pairLine.find(":")+2);    
    headerMap[key] = value;
  }

  //parse and save header info in map
  void parseHeader(){
    std::string tempLines = this->header.substr(this->header.find("\r\n")+2);
    while(tempLines.find("\r\n\r\n")!=std::string::npos){
      parsePair(tempLines.substr(0,tempLines.find("\r\n")));
      tempLines = tempLines.substr(tempLines.find("\r\n")+2);
    }
    bool ifCacheCtrl = headerMap.find("Cache-Control")==headerMap.end()?false:true;
    if(!ifCacheCtrl){
      //no field cache-control
      ifNoCache = false;
      ifNoStore = false;
      maxAge = 0;
      maxStale = 0;
      minFresh = 0;
    }
    else{
      //has field cache-control
      std::string cacheControl = headerMap.find("Cache-Control")->second;
      ifNoCache = cacheControl.find("no-cache")==std::string::npos? false:true;
      ifNoStore = cacheControl.find("no-store")==std::string::npos? false:true;
      //get max-age
      maxAge = getAgeInfo(cacheControl,"max-age");
      maxStale = getAgeInfo(cacheControl,"max-stale");
      minFresh = getAgeInfo(cacheControl,"min-fresh");
    }
    
  }

  int getAgeInfo(std::string cacheControl, std::string targetString){
    if(cacheControl.find(targetString)==std::string::npos){
      return 0;
    }
    else{
      std::string temp = cacheControl.substr(cacheControl.find(targetString)+targetString.size()+1);
      std::string str = temp.find_first_of(",")==std::string::npos?temp:temp.substr(0,temp.find_first_of(","));
      return stoi(str);
    }
  }
  
  std::string getMapValue(std::string key){
    if(this->headerMap.find(key)==this->headerMap.end()){
      return "";
      //throw std::exception();
    }
    return this->headerMap.find(key)->second;
  }
  
  virtual ~Message(){}
};

class Request: public Message{

private:
  std::string method;
  std::string ip;
  std::string port;
  std::string url;
  int uid;
  
public:
  explicit Request(std::string httpMessage):Message(httpMessage){
    this->method = this->headLine.substr(0,this->headLine.find_first_of(" "));
    bool ifHttps = this->headLine.find("http://")==std::string::npos;
    size_t start = ifHttps ? this->headLine.find_first_of(" ")+1 : this->headLine.find_first_of("http://")+7;
    std::string temp = this->headLine.substr(start);
    size_t end = ifHttps? temp.find_first_of(":") : temp.find_first_of("/");
    this->ip = temp.substr(0,end);
    this->port = ifHttps? "443":"80";

    size_t loc1 = this->headLine.find_first_of(" ")+1;
    std::string med = this->headLine.substr(loc1);
    size_t loc2 = med.find_first_of(" ")+1;
    this->url = med.substr(0,loc2-1);
  };

  void setUid(int id){
    this->uid = id;
  }

  int getUid(){
    return this->uid;
  }
  
  std::string getMethod(){
    return this->method;
  }
  std::string getIp(){
    return this->ip;
  }
  std::string getPort(){
    return this->port;
  }
  std::string getUrl(){
    return this->url;
  }
  virtual ~Request(){}
};

#endif
