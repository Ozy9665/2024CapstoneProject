#include "pch.h"
#include <iostream>
#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "ClientPacketHandler.h"

char sendData[] = "Hello World";

class ServerSession : public PacketSession
{
public:
	~ServerSession()
	{
		cout << "~ServerSession" << endl;
	}

	virtual void OnConnected() override
	{
		cout << "Connected To Server" << endl;
	}

	virtual void OnRecvPacket(BYTE* buffer, int32 len) override
	{
		ClientPacketHandler::HandlePacket(buffer, len);
	}

	virtual void OnSend(int32 len) override
	{
		//cout << "OnSend Len = " << len << endl;
	}

	virtual void OnDisconnected() override
	{
		//cout << "Disconnected" << endl;
	}
};

int main()
{
	this_thread::sleep_for(1s);

	SocketUtils::Init();

	ClientServiceRef service = make_shared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		make_shared<IocpCore>(),
		[]() { return make_shared<ServerSession>(); }, // TODO : SessionManager 등
		1);

	assert(service->Start());

	for (int32 i = 0; i < 5; i++)
	{
		GThreadManager->Launch([=]()
			{
				while (true)
				{
					service->GetIocpCore()->Dispatch();
				}
			});
	}

	GThreadManager->Join();
	SocketUtils::Clear();
}



//// 클라
//// 1) 소켓 생성
//// 2) 서버에 연결 요청
//// 3) 통신
//
//int main()
//{
//	SocketUtils::Init();
//
//	SOCKET clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
//	if (clientSocket == INVALID_SOCKET)
//		return 0;
//
//	u_long on = 1;
//	if (::ioctlsocket(clientSocket, FIONBIO, &on) == INVALID_SOCKET)
//		return 0;
//
//	SOCKADDR_IN serverAddr;
//	serverAddr.sin_family = AF_INET;
//	::inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
//	serverAddr.sin_port = ::htons(7777); // 80 : HTTP
//
//	// Connect
//	while (true)
//	{
//		if (::connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
//		{
//			if (::WSAGetLastError() == WSAEWOULDBLOCK)
//				continue;
//
//			// 이미 연결된 상태?
//			if (::WSAGetLastError() == WSAEISCONN)
//				break;
//		}
//	}
//
//	// Send
//	while (true)
//	{
//		//char sendBuffer[100] = "Hello I am Client!";
//
//		char sendBuffer[100];
//		cout << "Message : ";
//		cin.getline(sendBuffer, sizeof(sendBuffer));
//
//		int32 sendLen = sizeof(sendBuffer);
//
//		if (::send(clientSocket, sendBuffer, sendLen, 0) == SOCKET_ERROR)
//		{
//			if (::WSAGetLastError() == WSAEWOULDBLOCK)
//				continue;
//		}
//
//		cout << "Send Data! Len = " << sendLen << endl;
//
//		this_thread::sleep_for(1s);
//	}
//
//	::WSACleanup();
//
//}