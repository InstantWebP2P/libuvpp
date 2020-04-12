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
   if ((5 != argc) || (0 == atoi(argv[1])) ||
       (0 == atoi(argv[3])) || (0 == atoi(argv[4])))
   {
      cout << "usage: app2p local_port peer_ip peer_port peer_port" << endl;
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

   // local client address
   if (0 != getaddrinfo(NULL, argv[1], &hints, &local))
   {
      cout << "incorrect local port: " << argv[1] << endl;
      return 0;
   }

   UDTSOCKET client  = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);
   UDTSOCKET client1 = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

   // UDT Options
   //UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //UDT::setsockopt(client, 0, UDT_MSS, new int(9000), sizeof(int));
   //UDT::setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(client, 0, UDP_SNDBUF, new int(10000000), sizeof(int));

   // Windows UDP issue
   // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
   #ifdef WIN32
      UDT::setsockopt(client,  0, UDT_MSS, new int(1052), sizeof(int));
      UDT::setsockopt(client1, 0, UDT_MSS, new int(1052), sizeof(int));
   #endif

   // for rendezvous connection, enable the code below
   UDT::setsockopt(client, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));
   if (UDT::ERROR == UDT::bind(client, local->ai_addr, local->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   UDT::setsockopt(client1, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));
   if (UDT::ERROR == UDT::bind(client1, local->ai_addr, local->ai_addrlen))
   {
      cout << "bind1: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(local);

   // server/peer address
   if (0 != getaddrinfo(argv[2], argv[3], &hints, &peer))
   {
      cout << "incorrect peer address. " << argv[2] << ":" << argv[3] << endl;
      return 0;
   }

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(peer);

   // server/peer1 address
   if (0 != getaddrinfo(argv[2], argv[4], &hints, &peer))
   {
      cout << "incorrect peer1 address. " << argv[2] << ":" << argv[4] << endl;
      return 0;
   }

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(client1, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect1: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(peer);

   // using CC method
   //CUDPBlast* cchandle = NULL;
   //int temp;
   //UDT::getsockopt(client, 0, UDT_CC, &cchandle, &temp);
   //if (NULL != cchandle)
   //   cchandle->setRate(500);

   #ifndef WIN32
      pthread_create(new pthread_t, NULL, monitor, &client);
      pthread_create(new pthread_t, NULL, monitor, &client1);
   #else
      CreateThread(NULL, 0, monitor, &client, 0, NULL);
      CreateThread(NULL, 0, monitor, &client1, 0, NULL);
   #endif

   for (int i = 0;; i ++)
   {
      int size, ssize = 0;
      int ss;
      static char data[68] = {0};


      //usleep(10);

      // say ping/pong to peer
      if (!data[0]) {
          memcpy(data, "<ping", strlen("<ping"));
      } else if (!strncmp(data, "<ping", sizeof("<ping"))) {
          memcpy(data, "pong>", strlen("pong>"));
      } else if (!strncmp(data, "pong>", sizeof("pong>"))) {
          memcpy(data, "<ping", strlen("<ping"));
      }

      // send traffic to peer0
      size = strlen(data) + 1;
      while (ssize < size)
      {
         if (UDT::ERROR == (ss = UDT::send(client, data + ssize, size - ssize, 0)))
         {
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         ssize += ss;
      }

      if (ssize < size)
         break;

      //cout << "send" << i << ": " << string(data) << endl;

      // get ping/pong from peer
      ssize = 0;
      size  = strlen(data) + 1;
      while (ssize < size)
      {
         if (UDT::ERROR == (ss = UDT::recv(client, data + ssize, size - ssize, 0)))
         {
            cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         ssize += ss;
      }

      if (ssize < size)
         break;

      //cout << "recv" << i << ": " << string(data) << endl;

      // send traffic to peer1
      size = strlen(data) + 1;
      while (ssize < size)
      {
         if (UDT::ERROR == (ss = UDT::send(client1, data + ssize, size - ssize, 0)))
         {
            cout << "send1:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         ssize += ss;
      }

      if (ssize < size)
         break;

      //cout << "send" << i << ": " << string(data) << endl;

      // get ping/pong from peer1
      ssize = 0;
      size  = strlen(data) + 1;
      while (ssize < size)
      {
         if (UDT::ERROR == (ss = UDT::recv(client1, data + ssize, size - ssize, 0)))
         {
            cout << "recv1:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         ssize += ss;
      }

      if (ssize < size)
         break;

      //cout << "recv" << i << ": " << string(data) << endl;
   }

   UDT::close(client);
   UDT::close(client1);

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
}
