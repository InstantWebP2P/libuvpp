#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#include <iostream>
#include <udt.h>
#include "cc.h"

using namespace std;

#ifndef WIN32
void* recvdata(void*);
#else
DWORD WINAPI recvdata(LPVOID);
#endif


int main(int argc, char* argv[])
{
   if ((2 != argc) && ((3 != argc) || (0 == atoi(argv[2]))))
   {
      cout << "usage: appub pub_ip [pub_port]" << endl;
      return 0;
   }

   // use this function to initialize the UDT library
   UDT::startup();

   addrinfo hints;
   addrinfo* res;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   string service("9000");
   if (3 == argc)
      service = argv[2];

   if (0 != getaddrinfo(argv[1], service.c_str(), &hints, &res))
   {
      cout << "illegal port number or port is busy.\n" << endl;
      return 0;
   }

   UDTSOCKET serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

   // UDT Options
   //UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //UDT::setsockopt(serv, 0, UDT_MSS, new int(9000), sizeof(int));
   //UDT::setsockopt(serv, 0, UDT_RCVBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(serv, 0, UDP_RCVBUF, new int(10000000), sizeof(int));

   if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(res);

   cout << "pub is ready at ip: " << string(argv[1]) << "port: " << service << endl;

   if (UDT::ERROR == UDT::listen(serv, 10))
   {
      cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   sockaddr_storage subaddr;
   int addrlen = sizeof(subaddr);

   UDTSOCKET recver;

   while (true)
   {
      if (UDT::INVALID_SOCK == (recver = UDT::accept(serv, (sockaddr*)&subaddr, &addrlen)))
      {
         cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }

      char subhost[NI_MAXHOST];
      char subservice[NI_MAXSERV];
      getnameinfo((sockaddr *)&subaddr, addrlen, subhost, sizeof(subhost), subservice, sizeof(subservice), NI_NUMERICHOST|NI_NUMERICSERV);
      cout << "new connection: " << subhost << ":" << subservice << endl;

      #ifndef WIN32
         pthread_t rcvthread;
         pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(recver));
         pthread_detach(rcvthread);
      #else
         CreateThread(NULL, 0, recvdata, new UDTSOCKET(recver), 0, NULL);
      #endif
   }

   UDT::close(serv);

   // use this function to release the UDT library
   UDT::cleanup();

   return 1;
}

#ifndef WIN32
void* recvdata(void* usocket)
#else
DWORD WINAPI recvdata(LPVOID usocket)
#endif
{
   UDTSOCKET recver = *(UDTSOCKET*)usocket;
   delete (UDTSOCKET*)usocket;

   char* data;
   int size = 16;
   data = new char[size];
   
   struct sockaddr local_name, peer_name;
   int    local_namelen, peer_namelen;


   // local address
   memset(&local_name, 0, sizeof(local_name));
   local_namelen = sizeof(local_name);

   if (0 != UDT::getsockname(recver, &local_name, &local_namelen))
   {
      cout << "incorrect local socket before connect."<< endl;
      return 0;
   }

   // peer address
   memset(&peer_name, 0, sizeof(peer_name));
   peer_namelen = sizeof(peer_name);

   if (0 != UDT::getpeername(recver, &peer_name, &peer_namelen))
   {
      cout << "incorrect peer socket before connect."<< endl;
      return 0;
   }

   // for bi-direction sub/pub connection, enable code below
/*   if (UDT::ERROR == UDT::connect(recver, &peer_name, peer_namelen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }
*/

   // pub data out to sub
   memset(data, '6', 8);
   memset(data + 8, '8', 7);
   data[15] = 0;

   while (true)
   {
      int rsize = 0;
      int rs;

      while (rsize < size)
      {
         if (UDT::ERROR == (rs = UDT::send(recver, data + rsize, size - rsize, 0)))
         {
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         rsize += rs;
      }

      if (rsize < size)
         break;
   }

   delete [] data;

   UDT::close(recver);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
