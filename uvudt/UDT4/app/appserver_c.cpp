/*
 * appserver_c.cpp
 * Copyright (c) 2007-2009 Road iSystem,Inc. All rights reserved.
 *
 *  Created on: Sep 12, 2010
 *      Author: tom zhou at iwebpp@gmail.com
 */

/*
 * History:
 * Sep 12, 2010:zs:Created.
 */

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
#include "../src/udtc.h"
#include "../src/cc_cwrapper.h"

using namespace std;

#ifndef WIN32
void* recvdata(void*);
#else
DWORD WINAPI recvdata(LPVOID);
#endif


int main(int argc, char* argv[])
{
   if ((1 != argc) && ((2 != argc) || (0 == atoi(argv[1]))))
   {
      cout << "usage: appserver [server_port]" << endl;
      return 0;
   }

   // use this function to initialize the UDT library
   udt_startup();

   addrinfo hints;
   addrinfo* res;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   string service("9000");
   if (2 == argc)
      service = argv[1];

   if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res))
   {
      cout << "illegal port number or port is busy.\n" << endl;
      return 0;
   }

   UDTSOCKET serv = udt_socket(res->ai_family, res->ai_socktype, res->ai_protocol);


   // UDT Options
   //udt_setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //udt_setsockopt(serv, 0, UDT_MSS, new int(9000), sizeof(int));
   //udt_setsockopt(serv, 0, UDT_RCVBUF, new int(10000000), sizeof(int));
   //udt_setsockopt(serv, 0, UDP_RCVBUF, new int(10000000), sizeof(int));


   if (UDT_ERROR == udt_bind(serv, res->ai_addr, res->ai_addrlen))
   {
      cout << "bind: " << udt_getlasterror_desc() << endl;
      return 0;
   }

   freeaddrinfo(res);


   cout << "server is ready at port: " << service << endl;

   if (UDT_ERROR == udt_listen(serv, 10))
   {
      cout << "listen: " << udt_getlasterror_desc() << endl;
      return 0;
   }


   sockaddr_storage clientaddr;
   int addrlen = sizeof(clientaddr);

   UDTSOCKET recver;

   while (true)
   {
      if (UDT_INVALID_SOCK == (recver = udt_accept(serv, (sockaddr*)&clientaddr, &addrlen)))
      {
         cout << "accept: " << udt_getlasterror_desc() << endl;
         return 0;
      }

      char clienthost[NI_MAXHOST];
      char clientservice[NI_MAXSERV];
      getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
      cout << "new connection: " << clienthost << ":" << clientservice << endl;

      #ifndef WIN32
         pthread_t rcvthread;
         pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(recver));
         pthread_detach(rcvthread);
      #else
         CreateThread(NULL, 0, recvdata, new UDTSOCKET(recver), 0, NULL);
      #endif
   }

   udt_close(serv);

   // use this function to release the UDT library
   udt_cleanup();

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
   int size = 100000;
   data = new char[size];

   while (true)
   {
      int rsize = 0;
      int rs;
      while (rsize < size)
      {
         if (UDT_ERROR == (rs = udt_recv(recver, data + rsize, size - rsize, 0)))
         {
            cout << "recv:" << udt_getlasterror_desc() << endl;
            break;
         }

         rsize += rs;
      }

      if (rsize < size)
         break;
   }

   delete [] data;

   udt_close(recver);

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
