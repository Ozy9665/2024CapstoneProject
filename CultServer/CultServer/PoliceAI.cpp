#include "PoliceAI.h"
#include <random>

std::thread AIThread;
std::default_random_engine dre(std::random_device{}());
std::uniform_int_distribution<int> PatrolUID(0, static_cast<int>(PatrolPoints.size()) - 1);

void StartAIWorker()
{
	AIThread = std::thread(AIWorkerLoop);
	AIThread.join();
}

void UpdatePoliceAI()
{
	// ���� ������ ���� �ؾ���.
	// ���� ���: -1
	if (TargetID == -1) {
		// �þ߿� cultist�� ������.
		FVector direction = CalculateDir();

		float Speed = 100.0f;
		AiState.PositionX += direction.x * Speed * 0.016f;
		AiState.PositionY += direction.y * Speed * 0.016f;
		AiState.RotationYaw = std::atan2(direction.y, direction.x) * 180.0f / ����;
	}
	// TODO: ����, ���� ������Ʈ ����
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

void AIWorkerLoop()
{
	UpdatePoliceAI();	
}

