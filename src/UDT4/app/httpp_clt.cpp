#ifndef WIN32
   #include <arpa/inet.h>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <udt.h>

using namespace std;

int main(int argc, char* argv[])
{
   if ((argc != 3) || (0 == atoi(argv[2])))
   {
      cout << "usage: recvfile server_ip server_port" << endl;
      return -1;
   }

   // use this function to initialize the UDT library
   UDT::startup();

   struct addrinfo hints, *peer;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   UDTSOCKET fhandle = UDT::socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);

   if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer))
   {
      cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
      return -1;
   }

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(fhandle, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   freeaddrinfo(peer);

   const char* http_hdr = "GET / HTTP/1.1\r\n"
   "Accept:text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
   "Accept-Charset:GB2312,utf-8;q=0.7,*;q=0.7Accept-Encoding:gzip, deflate\r\n"
   "Accept-Language:zh-cn,zh;q=0.5Connection:keep-aliveHost:localhost:5000\r\n"
   "User-Agent:Mozilla/5.0 (X11; Linux x86_64; rv:7.0.1) Gecko/20100101 Firefox/7.0.1\r\n\r\n";
   if (UDT::ERROR == UDT::send(fhandle, http_hdr, strlen(http_hdr), 0))
   {
      cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   char buf[4096];
   int recvsize = 0;
   if (UDT::ERROR == (recvsize = UDT::recv(fhandle, buf, sizeof(buf), 0)))
   {
      cout << "recv: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   cout << buf << endl;
   UDT::close(fhandle);

   // use this function to release the UDT library
   UDT::cleanup();

   return 0;
}
