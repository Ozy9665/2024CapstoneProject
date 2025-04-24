#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <array>
#include <unordered_map>
#include <WS2tcpip.h>
#include <thread>
#include "Protocol.h"
#include "error.h"
#include "PoliceAI.h"

#pragma comment (lib, "WS2_32.LIB")

int main()
{
	std::wcout.imbue(std::locale("korean"));	// 한국어로 출력

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (s_socket <= 0) {
		std::cout << "Failed to create socket" << std::endl;
		return 1;
	}
	else {
		std::cout << "Server Socket Created" << std::endl;
	}

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(SOCKADDR_IN));
	listen(s_socket, SOMAXCONN);
	INT addr_size = sizeof(SOCKADDR_IN);

	while (true) {
		auto c_socket = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&addr), &addr_size, NULL, NULL);
		if (c_socket == INVALID_SOCKET) {
			std::cout << "클라이언트 연결 실패, 에러 코드: " << WSAGetLastError() << std::endl;
		}
		g_users.try_emplace(client_id, client_id, c_socket);
		std::cout << "새로운 클라이언트가 연결되었습니다." << inet_ntoa(addr.sin_addr)
			<< " Port: " << ntohs(addr.sin_port) << std::endl;
		StartAIWorker();
	}

	closesocket(s_socket);
	WSACleanup();
}
