#pragma once
#include "Scene.h"

class Actor;
class Player;
class GameObject;
class UI;

struct PQNode
{
	PQNode(int32 cost, Vec2Int pos) : cost(cost), pos(pos){}

	bool operator<(const PQNode& other) const { return cost < other.cost; }
	bool operator>(const PQNode& other) const { return cost > other.cost; }

	int32 cost;
	Vec2Int pos;
};

class DevScene : public Scene
{
	using Super = Scene;
public:
	DevScene();
	virtual ~DevScene() override;

	virtual void Init() override;
	virtual void Update() override;
	virtual void Render(HDC hdc) override;

	virtual void AddActor(Actor* actor);
	virtual void RemoveActor(Actor* actor);

	void LoadMap();
	void LoadPlayer();
	void LoadMonster();
	void LoadProjectile();
	void LoadEffect();
	void LoadTilemap();

	template<typename T>
	T* SpawnObject(Vec2Int pos)
	{
		// Type-Trait
		auto isGameObject = std::is_convertible_v<T*, GameObject*>;
		assert(isGameObject);

		T* ret = new T();
		ret->SetCellPos(pos, true);
		AddActor(ret);

		ret->BeginPlay();

		return ret;
	}

	template<typename T>
	T* SpawnObjectAtRandomPos()
	{
		Vec2Int randPos = GetRandomCellPos();
		return SpawnObject<T>(randPos);
	}

public:
	void Handle_S_AddObject(Protocol::S_AddObject& pkt);
	void Handle_S_RemoveObject(Protocol::S_RemoveObject& pkt);

public:
	// 강의에서는 GetObject 라고 표기했지만
	// 나는 여기서 어떤 헤더파일의 매크로와 이름이 겹치는
	// 상황이 발생해서 _GetObject로 수정했다.
	GameObject* _GetObject(uint64 id);

	Player* FindClosestPlayer(Vec2Int cellPos);
	bool FindPath(Vec2Int src, Vec2Int dest, vector<Vec2Int>& path, int32 maxDepth = 10);

	bool CanGo(Vec2Int cellPos);
	Vec2 ConvertPos(Vec2Int cellPos);
	Vec2Int GetRandomCellPos();

private:
	void TickMonsterSpawn();

private:
	const int32 DESIRED_MONSTER_COUNT = 20;
	int32 _monsterCount = 0;

	class TilemapActor* _tilemapActor = nullptr;
};