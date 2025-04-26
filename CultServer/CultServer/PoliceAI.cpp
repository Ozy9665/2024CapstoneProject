#include "PoliceAI.h"
#include <random>
#include <atomic>
#include <chrono>

std::thread AIThread;
std::atomic<bool> bRunning{ false };

std::array<FVector, 4> PatrolPoints	// 임시
{
	FVector{110.f, -1100.f, 2770.f},
	FVector{160.f, -1050.f, 2770.f},
	FVector{120.f, -1000.f, 2770.f},
	FVector{ 90.f, -1070.f, 2770.f}
};
std::default_random_engine dre(std::random_device{}());
std::uniform_int_distribution<int> PatrolUID(0, static_cast<int>(PatrolPoints.size()) - 1);

int CurrentPatrolIndex = 0;
int TargetID{ 0 };
int my_ID;

void InitializeAISession(const int ai_id)
{
	my_ID = ai_id;
	g_users.try_emplace(ai_id, ai_id); // AI 세션 생성
	for (auto& [id, session] : g_users)
	{
		if (id != ai_id && session.isValid())
		{
			session.do_send_connection(connectionHeader, ai_id, g_users[ai_id].getRole());
		}
	}

	if (not bRunning) {
		StartAIWorker();
	}
}

void StartAIWorker()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	bRunning = true;
	AIThread = std::thread(AIWorkerLoop);	
}

void StopAIWorker()
{
	bRunning = false;
	if (AIThread.joinable())
		AIThread.join();
}

void UpdatePoliceAI()
{
	// 순찰 모드: 0
	if (TargetID == 0) {
		// 시야에 cultist가 없으면.	(임시)
		FVector direction = CalculateDir();
			
		float Speed = 100.0f;
		AiState.PositionX += direction.x * Speed * 0.016f;
		AiState.PositionY += direction.y * Speed * 0.016f;
		AiState.RotationYaw = std::atan2(direction.y, direction.x) * 180.0f / 파이;
	}
	// TODO: 탐색, 추적, 상태 업데이트 로직

	// 저장 후 send
	g_users[my_ID].setPoliceState(AiState);
	BroadcastPoliceAIState();
}

FVector CalculateDir() {
	FVector direction{
			PatrolPoints[CurrentPatrolIndex].x - AiState.PositionX,
			PatrolPoints[CurrentPatrolIndex].y - AiState.PositionY,
			PatrolPoints[CurrentPatrolIndex].z - AiState.PositionZ
	};

	float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y);
	if (distance < 5.0f)
	{
		int OldIndex = CurrentPatrolIndex;
		while (CurrentPatrolIndex == OldIndex && PatrolPoints.size() > 1) {
			CurrentPatrolIndex = PatrolUID(dre);
		}
		return CalculateDir();
	}
	float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
	if (len != 0)
	{
		direction.x /= len;
		direction.y /= len;
	}

	return direction;
}	  

void BroadcastPoliceAIState()
{
	const FPoliceCharacterState& state = g_users[my_ID].getPoliceState();

	for (auto& [id, session] : g_users)
	{
		if (id == my_ID) continue;

		if (session.isValid())
		{
			std::cout << "[AI Pos] id: " << state.PlayerID << "X: " << state.PositionX << "Y: " << state.PositionY << "\n";
			session.do_send_data(policeHeader, &state, sizeof(FPoliceCharacterState));
		}
	}
}

void AIWorkerLoop()
{
	const auto frameDuration = std::chrono::duration<float>(1.0f / 60.0f); // 목표 프레임: 1/60초
	auto prev = std::chrono::high_resolution_clock::now();

	while (bRunning)
	{
		auto frameStart = std::chrono::high_resolution_clock::now();
		float DeltaTime = std::chrono::duration<float>(frameStart - prev).count();
		prev = frameStart;

		UpdatePoliceAI();

		auto frameEnd = std::chrono::high_resolution_clock::now();
		auto elapsed = frameEnd - frameStart;

		if (elapsed < frameDuration)
		{
			std::this_thread::sleep_for(frameDuration - elapsed);
		}
	}
}
