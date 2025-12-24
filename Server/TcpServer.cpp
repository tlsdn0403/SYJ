#include <iostream>
#include "Error.h"

#define SERVERPORT	7777
#define BUFSIZE		512

int main(int argc, char* argv[])
{
	int rv;		// rv는 return value

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);	// 대기 소켓
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));	// serveraddr구조체의 메모리를 0으로 채움.
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	rv = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (rv == SOCKET_ERROR) err_quit("bind()");

	// listen()
	rv = listen(listen_sock, SOMAXCONN); // 대기열 큐의 최대크기를 약 21억으로 지정.
	if (rv == SOCKET_ERROR) err_quit("listen()");

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;	// 클라소켓
	struct sockaddr_in clientaddr;
	int addrlen;
	char buf[BUFSIZE + 1];

	while (1) {		// 새로운 클라이언트가 접속할때마다 받아야하므로 무한루프
		// accept
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// 접속한 클라이언트 정보 출력
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		std::cout << "[TCP 서버] 클라이언트 접속 : IP 주소 = " << addr
			<< ", 포트 번호 = " << ntohs(clientaddr.sin_port) << '\n';

		// 클라이언트와 데이터 통신
		while (1) {
			// 데이터 받기
			rv = recv(client_sock, buf, BUFSIZE, 0);
			if (rv == SOCKET_ERROR) {
				err_display("recv()");
				break;
			}
			else if (rv == 0)
				break;

			// 받은 데이터 출력
			buf[rv] = '\0';	// 문자열이 끝났다는걸 알려주기 위해 널 종료 문자를 붙임.
			std::cout << "[TCP/" << addr << ":" << ntohs(clientaddr.sin_port) << "] "
				<< buf << '\n';

			// 데이터 보내기
			rv = send(client_sock, buf, rv, 0);
			if (rv == SOCKET_ERROR) {
				err_display("send()");
				break;
			}
		}

		// 클라 소켓 닫기
		closesocket(client_sock);
		std::cout << "[TCP 서버] 클라이언트 종료 : IP 주소 = " << addr
			<< ", 포트 번호 = " << ntohs(clientaddr.sin_port) << '\n';
	}

	// 대기 소켓 닫기
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}