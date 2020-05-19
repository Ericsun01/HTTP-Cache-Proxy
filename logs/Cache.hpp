#ifndef CACHE_HPP
#define CACHE_HPP

#include <ctime>
#include <mutex>
#include <unistd.h>
#include <string>
#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include "Message.hpp"

std::mutex cacheMutex;

class LRUCache{
private:
  size_t cacheSize;
  size_t cacheCapacity;
  std::list<std::pair<std::string,Message>> orderList;
  std::unordered_map<std::string,std::list<std::pair<std::string,Message>>::iterator> hsMap;

public:
  LRUCache(size_t capacity):cacheSize(0),cacheCapacity(capacity){}
  
  void add(std::string key, Message value){
    if(hsMap.find(key)!=hsMap.end()){
      orderList.erase(hsMap[key]);
      cacheSize--;
    }
    orderList.push_front(make_pair(key, value));
    hsMap[key] = orderList.begin();
    cacheSize++;
    if(cacheSize>cacheCapacity){
      hsMap.erase(orderList.back().first);
      orderList.pop_back();
      cacheSize--;
    }
  }

  bool ifInCache(std::string key){
    if(hsMap.find(key)!=hsMap.end()){
      return true;
    }
    else{
      return false;
    }
  }
  
  Message get(std::string key){
    if(hsMap.find(key)==hsMap.end()){
      throw std::exception();
    }
    std::pair<std::string,Message> msg = make_pair(key,hsMap[key]->second);
    orderList.erase(hsMap[key]);
    orderList.push_front(msg);
    hsMap[key]=orderList.begin();
    return hsMap[key]->second;
  }

  void getValidateReq(Request &request, Message &response){
    std::string value;
    std::string kv;
    if((value = response.getMapValue("ETag"))!=""){
      kv = "If-None-Match: " + value;
      request.insertToHeader(kv);
    }
    if((value = response.getMapValue("Last-Modified"))!=""){
      kv = "If-Modified-Since: " + value;
      request.insertToHeader(kv);
    }
  }

  
  bool checkIfValid(Request &request){
    std::string reqTime = request.getMapValue("Date");
    time_t currTime = stringToTime(reqTime);
    Message response = get(request.getUrl());
    std::string resTime = response.getMapValue("Date");
    time_t cacheTime = stringToTime(resTime);

    int reqLifeTime = request.getMaxAge()+request.getMaxStale()-request.getMinFresh();
    int resLifeTime = response.getMaxAge()+response.getMaxStale()-request.getMinFresh();
    int lifeTime = reqLifeTime>resLifeTime?resLifeTime:reqLifeTime;

    if(cacheTime+lifeTime<currTime){
      return false;
    }
    else{
      return true;
    }
  }

  time_t stringToTime(std::string str) {
    tm tm;
    strptime(str.c_str(), "%a, %d %b %Y %H:%M:%S", &tm);
    time_t t = mktime(&tm);
    return t;
  }

  std::string getCurrTime() { 
    time_t currTime = time(0);
    tm *tm = localtime(&currTime);
    return std::string(asctime(tm));
  }
  
  ~LRUCache(){}
};

#endif
