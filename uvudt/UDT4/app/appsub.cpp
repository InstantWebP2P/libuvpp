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
void* monitor(void*);
#else
DWORD WINAPI monitor(LPVOID);
#endif

int main(int argc, char* argv[])
{
   if ((4 != argc) || (0 == atoi(argv[3]) || (0 == atoi(argv[1]))))
   {
      cout << "usage: appsub sub_port pub_ip pub_port" << endl;
      return 0;
   }

   // use this function to initialize the UDT library
   UDT::startup();

   struct addrinfo hints, *local, *peer;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   if (0 != getaddrinfo(NULL, argv[1], &hints, &local))
   {
      cout << "incorrect network address.\n" << endl;
      return 0;
   }

#ifndef WIN32
   //pthread_create(new pthread_t, NULL, monitor, &sub);
    pthread_create(new pthread_t, NULL, monitor, local);
#else
   CreateThread(NULL, 0, monitor, &sub, 0, NULL);
#endif

   UDTSOCKET sub = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

   // UDT Options
   //UDT::setsockopt(sub, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //UDT::setsockopt(sub, 0, UDT_MSS, new int(9000), sizeof(int));
   //UDT::setsockopt(sub, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(sub, 0, UDP_SNDBUF, new int(10000000), sizeof(int));

   // Windows UDP issue
   // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
   #ifdef WIN32
      UDT::setsockopt(sub, 0, UDT_MSS, new int(1052), sizeof(int));
   #endif

   // for bi-direction clinet/server connection, enable the code below
   //UDT::setsockopt(sub, 0, UDT_REUSEADDR, new bool(true), sizeof(bool));
   if (UDT::ERROR == UDT::bind(sub, local->ai_addr, local->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(local);

   if (0 != getaddrinfo(argv[2], argv[3], &hints, &peer))
   {
      cout << "incorrect pub/peer address. " << argv[2] << ":" << argv[3] << endl;
      return 0;
   }

   // connect to the pub
   if (UDT::ERROR == UDT::connect(sub, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(peer);

   // using CC method
   //CUDPBlast* cchandle = NULL;
   //int temp;
   //UDT::getsockopt(sub, 0, UDT_CC, &cchandle, &temp);
   //if (NULL != cchandle)
   //   cchandle->setRate(500);

   int size = 16;
   char* data = new char[size];

   for (int i = 0; i < 1000000; i ++)
   {
      int ssize = 0;
      int ss;
      while (ssize < size)
      {
         if (UDT::ERROR == (ss = UDT::recv(sub, data + ssize, size - ssize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         ssize += ss;
      }

      if (ssize < size)
         break;

      //cout << "recv: " << string(data) << endl;
   }

   UDT::close(sub);

   delete [] data;

   // use this function to release the UDT library
   UDT::cleanup();

   return 1;
}

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
#if 0
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT::TRACEINFO perf;

   cout << "SendRate(Mb/s)\tRTT(ms)\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK" << endl;

   while (true)
   {
      #ifndef WIN32
         sleep(8);
      #else
         Sleep(8000);
      #endif

      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
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
#else
      struct addrinfo local = *(struct addrinfo *)s;

      UDTSOCKET serv = UDT::socket(local.ai_family, local.ai_socktype, local.ai_protocol);

      // for bi-direction clinet/server connection, enable the code below
      //UDT::setsockopt(serv, 0, UDT_REUSEADDR, new bool(true), sizeof(bool));
      if (UDT::ERROR == UDT::bind(serv, local.ai_addr, local.ai_addrlen))
      {
         cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }

      // listen
      if (UDT::ERROR == UDT::listen(serv, 10))
      {
         cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
         return 0;
      }

      sockaddr_storage clientaddr;
      int addrlen = sizeof(clientaddr);

      UDTSOCKET recver;

      while (true)
      {
         if (UDT::INVALID_SOCK == (recver = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen)))
         {
            cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
            return 0;
         }

         char clienthost[NI_MAXHOST];
         char clientservice[NI_MAXSERV];
         getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
         cout << "new connection: " << clienthost << ":" << clientservice << endl;
      }

      UDT::close(serv);

      return NULL;
#endif
}
