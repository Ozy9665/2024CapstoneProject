#include "Protocol.h"

std::unordered_map<int, SESSION> g_users;
int client_id = 0;

void CALLBACK g_send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
{
	EXP_OVER* o = reinterpret_cast<EXP_OVER*>(p_over);
	delete o;
}

void CALLBACK g_recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
{
	auto my_id = reinterpret_cast<long long>(p_over->hEvent);
	g_users[my_id].recv_callback(err, num_bytes, p_over, flag);
}
