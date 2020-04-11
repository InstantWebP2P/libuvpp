/*
 * appclient_c.cpp
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
void* monitor(void*);
#else
DWORD WINAPI monitor(LPVOID);
#endif

int main(int argc, char* argv[])
{
   if ((3 != argc) || (0 == atoi(argv[2])))
   {
      cout << "usage: appclient server_ip server_port" << endl;
      return 0;
   }

   // use this function to initialize the UDT library
   udt_startup();

   struct addrinfo hints, *local, *peer;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   if (0 != getaddrinfo(NULL, "9000", &hints, &local))
   {
      cout << "incorrect network address.\n" << endl;
      return 0;
   }

   UDTSOCKET client = udt_socket(local->ai_family, local->ai_socktype, local->ai_protocol);

   // UDT Options
   //udt_setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //udt_setsockopt(client, 0, UDT_MSS, new int(9000), sizeof(int));
   //udt_setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
   //udt_setsockopt(client, 0, UDP_SNDBUF, new int(10000000), sizeof(int));

   // Windows UDP issue
   // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
   #ifdef WIN32
      udt_setsockopt(client, 0, UDT_MSS, new int(1052), sizeof(int));
   #endif

   // for rendezvous connection, enable the code below
   /*
   udt_setsockopt(client, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));
   if (UDT_ERROR == udt_bind(client, local->ai_addr, local->ai_addrlen))
   {
      cout << "bind: " << udt_getErrorMessage() << endl;
      return 0;
   }
  */

   freeaddrinfo(local);

   if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer))
   {
      cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
      return 0;
   }

   // connect to the server, implict bind
   if (UDT_ERROR == udt_connect(client, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << udt_getlasterror_desc() << endl;
      return 0;
   }

   freeaddrinfo(peer);

   // using CC method
   //CUDPBlast* cchandle = NULL;
   //int temp;
   //udt_getsockopt(client, 0, UDT_CC, &cchandle, &temp);
   //if (NULL != cchandle)
   //   cchandle->setRate(500);

   int size = 100000;
   char* data = new char[size];

   #ifndef WIN32
      pthread_create(new pthread_t, NULL, monitor, &client);
   #else
      CreateThread(NULL, 0, monitor, &client, 0, NULL);
   #endif

   for (int i = 0; i < 1000000; i ++)
   {
      int ssize = 0;
      int ss;
      while (ssize < size)
      {
         if (UDT_ERROR == (ss = udt_send(client, data + ssize, size - ssize, 0)))
         {
            cout << "send:" << udt_getlasterror_desc() << endl;
            break;
         }

         ssize += ss;
      }

      if (ssize < size)
         break;
   }

   udt_close(client);

   delete [] data;

   // use this function to release the UDT library
   udt_cleanup();

   return 1;
}

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT_TRACEINFO perf;

   cout << "SendRate(Mb/s)\tRTT(ms)\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK" << endl;

   while (true)
   {
      #ifndef WIN32
         sleep(1);
      #else
         Sleep(1000);
      #endif

      if (UDT_ERROR == udt_perfmon(u, &perf, true))
      {
         cout << "perfmon: " << udt_getlasterror_desc() << endl;
         break;
      }

      cout << perf.mbpsSendRate << "\t\t"
           << perf.msRTT << "\t"
           << perf.pktCongestionWindow << "\t"
           << perf.usPktSndPeriod << "\t\t\t"
           << perf.pktRecvACK << "\t"
           << perf.pktRecvNAK << endl;
   }

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
