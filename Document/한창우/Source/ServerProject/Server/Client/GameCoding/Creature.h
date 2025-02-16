#pragma once
#include "GameObject.h"

class Flipbook;
class Collider;
class BoxCollider;

struct Stat
{
	int32 hp = 100;
	int32 maxHp = 100;
	int32 attack = 30;
	int32 defence = 5;
	float speed = 0;
};

class Creature : public GameObject
{
	using Super = GameObject;

public:
	Creature();
	virtual ~Creature() override;

	virtual void BeginPlay() override;
	virtual void Tick() override;
	virtual void Render(HDC hdc) override;

private:
	virtual void TickIdle() override {};
	virtual void TickMove() override {};
	virtual void TickSkill() override {};
	virtual void UpdateAnimation() override {};

public:
	virtual void OnDamaged(Creature* attacker);

	void SetStat(Stat stat) { _stat = stat; }
	Stat& GetStat() { return _stat; }

protected:
	Stat _stat;
};

