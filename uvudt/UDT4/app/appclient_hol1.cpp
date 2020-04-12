#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <netinet/in.h>
   #include <arpa/inet.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#include <iostream>
#include <udt.h>
#include "cc.h"
#include "test_util.h"

using namespace std;

#ifndef WIN32
void* monitor(void*);
void* worker(void*);
#else
DWORD WINAPI monitor(LPVOID);
DWORD WINAPI worker(LPVOID);
#endif

int main(int argc, char* argv[])
{
   if ((3 != argc) || (0 == atoi(argv[2])))
   {
      cout << "usage: appclient server_ip server_port" << endl;
      return 0;
   }

   // Automatically start up and clean up UDT module.
   UDTUpDown _udt_;

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

   UDTSOCKET client = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);
   UDTSOCKET client1 = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);
   int client_osfd, optlen;
   // get Osfd
   UDT::getsockopt(client, 0, UDT_OSFD, &client_osfd, &optlen);
   cout << "client osfd: " << client_osfd << endl;

   UDT::getsockopt(client1, 0, UDT_OSFD, &client_osfd, &optlen);
   cout << "client1 osfd: " << client_osfd << endl;

#if 0
   // binding on same port
   struct sockaddr_in ip4addr;

   ip4addr.sin_family = AF_INET;
   ip4addr.sin_port = htons(6868);
   inet_pton(AF_INET, "127.0.0.1", &ip4addr.sin_addr);

   UDT::bind(client, (struct sockaddr *)&ip4addr, sizeof(ip4addr));
   UDT::bind(client1, (struct sockaddr *)&ip4addr, sizeof(ip4addr));
#endif

   // UDT Options
   //UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //UDT::setsockopt(client, 0, UDT_MSS, new int(9000), sizeof(int));
   //UDT::setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(client, 0, UDP_SNDBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(client, 0, UDT_MAXBW, new int64_t(12500000), sizeof(int));

   // Windows UDP issue
   // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
   #ifdef WIN32
      UDT::setsockopt(client, 0, UDT_MSS, new int(1052), sizeof(int));
      UDT::setsockopt(client1, 0, UDT_MSS, new int(1052), sizeof(int));
   #endif

   // for rendezvous connection, enable the code below
   /*
   UDT::setsockopt(client, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));
   if (UDT::ERROR == UDT::bind(client, local->ai_addr, local->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
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
   if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

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

      pthread_create(new pthread_t, NULL, worker, &client);
      pthread_create(new pthread_t, NULL, worker, &client1); 
   #else
      CreateThread(NULL, 0, monitor, &client, 0, NULL);
      CreateThread(NULL, 0, monitor, &client1, 0, NULL);

      CreateThread(NULL, 0, worker, &client, 0, NULL);
      CreateThread(NULL, 0, worker, &client1, 0, NULL);
   #endif

   while (1) sleep(2);

   return 0;
}

#ifndef WIN32
void* worker(void* s)
#else
DWORD WINAPI worker(LPVOID s)
#endif
{
   UDTSOCKET client = *(UDTSOCKET*)s;

   int size = 100000;
   char* data = new char[size];


   int pause = 0;
   for (int i = 0; i < 10000000; i ++)
   {
      if (i == 999999) printf("sending done ...\n");

      int ssize = 0;
      int ss;
      while (ssize < size)
      {
         ///if (pause == 0)
         if (UDT::ERROR == (ss = UDT::send(client, data + ssize, size - ssize, 0)))
         {
            ///cout << "send:" << UDT::getlasterror().getErrorMessage() << ", errocde: " << UDT::getlasterror_code() << endl;
            if (UDT::getlasterror_code() != UDT::ERRORINFO::ETIMEOUT) {
                cout << "send:" << UDT::getlasterror().getErrorMessage() << ", errocde: " << UDT::getlasterror_code() << endl;
                break;
            } else {
                pause = 3000;
            }
         }
        
         if (pause) pause--;
 
         ssize += ss;
      }

      if (ssize < size) {
         printf("unmatched size\n");
         ///break;
      }
   }

   UDT::close(client);
   delete [] data;

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
   UDTSOCKET u = *(UDTSOCKET*)s;

   UDT::TRACEINFO perf;

   cout << "SendRate(Mb/s)\tRTT(ms)\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK\tbyteAvailRcvBuf\tbyteAvailSndBuf" << endl;

   while (true)
   {
      #ifndef WIN32
         sleep(1);
      #else
         Sleep(1000);
      #endif

      if (UDT::ERROR == UDT::perfmon(u, &perf))
      {
         cout << "perfmon-" << u << ": " << UDT::getlasterror().getErrorMessage() << endl;
         break;
      }

      cout << u << ": "
           << perf.mbpsSendRate << "\t\t" 
           << perf.msRTT << "\t" 
           << perf.pktCongestionWindow << "\t" 
           << perf.usPktSndPeriod << "\t\t\t" 
           << perf.pktRecvACK << "\t" 
           << perf.pktRecvNAK << "\t"
           << perf.byteAvailRcvBuf << "\t"
           << perf.byteAvailSndBuf << endl;
   }

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}
