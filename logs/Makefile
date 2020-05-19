SOURCES = main.cpp #mySocket.cpp myHTTP.cpp #Cache.cpp
OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CPPFLAGS = -ggdb3 -Wall -Werror -pedantic -std=gnu++11

http_caching_proxy: $(OBJS)
	g++ $(CPPFLAGS) -o proxy $(OBJS) -lpthread
%.o: %.cpp  #common.h common.h myHTTP.h Cache.h myexception.h
	g++ $(CPPFLAGS) -c $< -lpthread
  
clean:
	rm -rf *~ proxy *.o *.txt
