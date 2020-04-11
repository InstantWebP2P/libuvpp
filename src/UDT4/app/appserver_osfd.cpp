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
#include <map>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <udt.h>
#include "cc.h"
#include "test_util.h"

using namespace std;

#ifndef WIN32
void* recvdata(void*, int);
#else
DWORD WINAPI recvdata(LPVOID, SOCKET);
#endif

int main(int argc, char* argv[])
{
	if ((1 != argc) && ((2 != argc) || (0 == atoi(argv[1]))))
	{
		cout << "usage: appserver [server_port]" << endl;
		return 0;
	}

	// Automatically start up and clean up UDT module.
	UDTUpDown _udt_;

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

	UDTSOCKET serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	int32_t serv_osfd, optlen;

	// get Osfd
	UDT::getsockopt(serv, 0, UDT_OSFD, &serv_osfd, &optlen);
	cout << "serv osfd: " << serv_osfd << endl;

	// UDT Options
	//UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
	//UDT::setsockopt(serv, 0, UDT_MSS, new int(9000), sizeof(int));
	//UDT::setsockopt(serv, 0, UDT_RCVBUF, new int(10000000), sizeof(int));
	//UDT::setsockopt(serv, 0, UDP_RCVBUF, new int(10000000), sizeof(int));

	// set non-blocking
	bool sync = false;
	UDT::setsockopt(serv, 0, UDT_SNDSYN, &sync, sizeof(sync));
	UDT::setsockopt(serv, 0, UDT_RCVSYN, &sync, sizeof(sync));

	if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen))
	{
		cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
		return 0;
	}

	freeaddrinfo(res);

	cout << "server is ready at port: " << service << endl;

	if (UDT::ERROR == UDT::listen(serv, 10))
	{
		cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
		return 0;
	}

	sockaddr_storage clientaddr;
	int addrlen = sizeof(clientaddr);

	UDTSOCKET recver;
	int recver_osfd;
#if 0
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

#ifndef WIN32
		pthrecv_t rcvthrecv;
		pthrecv_create(&rcvthrecv, NULL, recvdata, new UDTSOCKET(recver));
		pthrecv_detach(rcvthrecv);
#else
		CreateThrecv(NULL, 0, recvdata, new UDTSOCKET(recver), 0, NULL);
#endif
	}
#else
#define MAX_EVENTS (256)

	std::map<int, UDTSOCKET> OsfdCnt;
	OsfdCnt[serv_osfd] = serv; // serv pair

	struct epoll_event ev, events[MAX_EVENTS];
	int kdpfd = epoll_create(256);
	fcntl(kdpfd, F_SETFD, FD_CLOEXEC);

	// add serv Osfd in epoll loop
	///ev.events = EPOLLIN;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = serv_osfd;
	if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, serv_osfd, &ev) < 0) {
		fprintf(stderr, "epoll set insertion error: fd=%d",
				serv_osfd);
		return -1;
	}

	cout << "enter event loop" << endl;
	int sevr_r = 0, sevr_e = 0, clnt_r = 0, clnt_e = 0, clnt_osr = 0, clnt_osw = 0, clnt_ose = 0;
	for(;;) {
		int nfds = epoll_wait(kdpfd, events, MAX_EVENTS, -1);
		for(int n = 0; n < nfds; ++n) {
			///cout << "event " << nfds << endl;
			if (events[n].data.fd == serv_osfd) {
				// record event
				sevr_e ++;

				// check real UDT event
				int32_t udt_event;
				UDT::getsockopt(serv, 0, UDT_EVENT, &udt_event, &optlen);
				if (udt_event & UDT_EPOLL_IN) {
					cout << "recv serv event " << endl;
					sevr_r ++;
					if (!(sevr_e % 10)) {
						cout << "recv serv event ratio: " << (sevr_r * 100 / sevr_e) << "%" << endl;
					}
				} else {
					cout << "not recv serv event " << endl;
					if (udt_event & UDT_EPOLL_OUT) {
						cout << "dummy write serv event " << endl;
					}
				}
				if (udt_event & UDT_EPOLL_OUT) {
					///cout << "write serv event " << endl;
				} else {
					cout << "not write serv event " << endl;
				}
				if (udt_event & UDT_EPOLL_ERR) {
					cout << "error serv event " << endl;
					// consume Os fd event
					char dummy;
					recv(serv_osfd, &dummy, sizeof(dummy), 0);

					UDT::close(serv);
					return -1;
				}

				// accept all of pending client until return EAGAIN
				while (1) {
					if (UDT::INVALID_SOCK == (recver = UDT::accept(serv, (sockaddr*)&clientaddr, &addrlen)))
					{
						if (UDT::getlasterror_code() == UDT::ERRORINFO::EASYNCRCV) {
							// consume Os fd event
							char dummy;
							recv(serv_osfd, &dummy, sizeof(dummy), 0);

							cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
							break;
						} else {
							cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
							return -1;
						}
					} else {
						cout << "accept UDTSOCKET: " << recver << endl;
					}

					char clienthost[NI_MAXHOST];
					char clientservice[NI_MAXSERV];
					getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST|NI_NUMERICSERV);
					cout << "new connection: " << clienthost << ":" << clientservice << endl;
					UDT::setsockopt(recver, 0, UDT_SNDSYN, &sync, sizeof(sync));
					UDT::setsockopt(recver, 0, UDT_RCVSYN, &sync, sizeof(sync));

					///setnonblocking(client);
					// get Osfd
					UDT::getsockopt(recver, 0, UDT_OSFD, &recver_osfd, &optlen);
					cout << "recver osfd: " << recver_osfd << endl;

					memset(&ev, 0, sizeof(ev));
					///ev.events = EPOLLIN;
					ev.events = EPOLLIN | EPOLLET;
					ev.data.fd = recver_osfd;
					if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, recver_osfd, &ev) < 0) {
						perror("epoll_ctl add\n");
						fprintf(stderr, "epoll set insertion error: fd=%d\n",
								recver_osfd);
						return -1;
					}

					// record fd map to UDTSOCKET
					OsfdCnt[recver_osfd] = recver;
				}
			} else {
				// record event
				clnt_e ++;

				// check Os fd event
				if (events[n].events & EPOLLIN) {
					clnt_osr ++;
					if (!(clnt_e % 1000)) {
						cout << "Os fd client read event ratio: " << (clnt_osr * 100 / clnt_e) << "%" << endl;
					}
				}
				if (events[n].events & EPOLLOUT) {
					clnt_osw ++;
					if (!(clnt_e % 1000)) {
						cout << "Os fd client write event ratio: " << (clnt_osw * 100 / clnt_e) << "%" << endl;
					}
				}
				if (events[n].events & EPOLLERR) {
					clnt_ose ++;
					if (!(clnt_e % 1000)) {
						cout << "Os fd client error event ratio: " << (clnt_ose * 100 / clnt_e) << "%" << endl;
					}
				}
				if (events[n].events & EPOLLHUP) {
					clnt_ose ++;
					if (!(clnt_e % 1000)) {
						cout << "Os fd client hup event ratio: " << (clnt_ose * 100 / clnt_e) << "%" << endl;
					}
				}

				int32_t udt_event;
				UDT::getsockopt(OsfdCnt[events[n].data.fd], 0, UDT_EVENT, &udt_event, &optlen);
				if (udt_event & UDT_EPOLL_IN) {
					clnt_r ++;
					if (!(clnt_e % 1000)) {
						cout << "recv client event ratio: " << (clnt_r * 100 / clnt_e) << "%" << endl;
					}
				} else {
					//cout << "not recv client event " << endl;
					if (udt_event & UDT_EPOLL_OUT) {
						//cout << "dummy write client event " << endl;
					}
				}
				if (udt_event & UDT_EPOLL_OUT) {
					///cout << "write client event " << endl;
				} else {
					cout << "not write client event " << endl;
				}
				if (udt_event & UDT_EPOLL_ERR) {
					cout << "error client event " << endl;
					// consume Os fd event
					char dummy;
					recv(events[n].data.fd, &dummy, sizeof(dummy), 0);

					UDT::close(OsfdCnt[events[n].data.fd]);
					continue;
				}

				//do_use_fd(events[n].data.fd);
				if (udt_event & UDT_EPOLL_IN) {
					recvdata(new UDTSOCKET(OsfdCnt[events[n].data.fd]), events[n].data.fd);
				} else {
					// consume Os fd event
					char dummy;
					recv(events[n].data.fd, &dummy, sizeof(dummy), 0);
				}
			}
		}
	}
#endif
	UDT::close(serv);

	return 0;
}

#ifndef WIN32
void* recvdata(void* usocket, int recver_osfd)
#else
		DWORD WINAPI recvdata(LPVOID usocket, SOCKET recver_osfd)
#endif
{
	UDTSOCKET recver = *(UDTSOCKET*)usocket;
	delete (UDTSOCKET*)usocket;

	char* data;
	int size = 100000;
	data = new char[size];

	///while (true)
	{
		int rsize = 0;
		int rs;
		while (rsize < size)
		{
			if (UDT::ERROR == (rs = UDT::recv(recver, data + rsize, size - rsize, 0)))
			{
				if (UDT::getlasterror_code() == UDT::ERRORINFO::EASYNCRCV) {
					// consume Os fd event
					char dummy;
					recv(recver_osfd, &dummy, sizeof(dummy), 0);

					///cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
					break;
				} else {
					cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
					///exit(-1);
					// consume Os fd event
					char dummy;
					recv(recver_osfd, &dummy, sizeof(dummy), 0);

					UDT::close(recver);
					break;
				}
			}

			rsize += rs;
		}

		if (rsize < size)
			;///break;
	}

	delete [] data;

	///UDT::close(recver);

#ifndef WIN32
	return NULL;
#else
	return 0;
#endif
}

