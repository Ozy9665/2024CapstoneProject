#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <array>
#include <unordered_map>
#include <WS2tcpip.h>
#include <thread>
#include <string>
#include "Protocol.h"
#include "error.h"
#include "PoliceAI.h"

#pragma comment (lib, "WS2_32.LIB")

SOCKET g_s_socket, g_c_socket;
EXP_OVER g_a_over;

void CommandWorker()
{
	while (true)
	{
		std::string command;
		std::getline(std::cin, command);

		if (command == "add_ai")
		{
			int ai_id = client_id++;
			InitializeAISession(ai_id);
			std::cout << "[Command] New AI added. ID: " << ai_id << "\n";
		}
		else if (command == "exit")
		{
			std::cout << "[Command] Exiting server.\n";
			exit(0);
		}
		else
		{
			std::cout << "[Command] Unknown command: " << command << "\n";
		}
	}
}

int main()
{
	HANDLE h_iocp;
	std::wcout.imbue(std::locale("korean"));	// 한국어로 출력

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	std::thread CommandThread(CommandWorker);

	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (g_s_socket <= 0) {
		std::cout << "Failed to create socket" << std::endl;
		return 1;
	}
	else {
		std::cout << "Server Socket Created" << std::endl;
	}

	SOCKADDR_IN addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	listen(g_s_socket, SOMAXCONN);
	INT addr_size = sizeof(SOCKADDR_IN);

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over.comp_type = OP_ACCEPT;

	while (true) {
		DWORD io_size;
		WSAOVERLAPPED* o;
		ULONG_PTR key;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &io_size, &key, &o, INFINITE);
		EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);
		if (FALSE == ret) {
			auto err_no = WSAGetLastError();
			print_error_message(err_no);
			if (g_users.count(key) != 0) {
				g_users.erase(key);
			}
		}
		if ((eo->comp_type == OP_RECV || eo->comp_type == OP_SEND) && io_size == 0) {
			if (g_users.count(key) != 0) {
				g_users.erase(key);
				delete eo;
				continue;
			}
		}	
		switch (eo->comp_type)
		{
		case OP_ACCEPT:
			break;
		case OP_SEND:
			break;
		case OP_RECV:
			break;
		}
	}

	CommandThread.join();
	StopAIWorker();
	closesocket(g_s_socket);
	WSACleanup();
}
