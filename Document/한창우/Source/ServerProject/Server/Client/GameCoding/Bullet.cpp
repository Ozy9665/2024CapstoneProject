#include "pch.h"
#include "Bullet.h"
#include "Utils.h"
#include "TimeManager.h"
#include "ObjectManager.h"
#include "SceneManager.h"
#include "UIManager.h"

Bullet::Bullet() : Object(ObjectType::Projectile)
{
}

Bullet::~Bullet()
{
}

void Bullet::Init()
{
	_radius = 20.f;
}

void Bullet::Update()
{
	float deltaTime = GET_SINGLE(TimeManager)->GetDeltaTime();

	// Wind
	float windPercent = GET_SINGLE(UIManager)->GetWindPercent();
	_speed.x += 20 * deltaTime * windPercent;

	// Gravity
	Vector g{ 0, 1000 };
	_speed += g * deltaTime;

	// 이동
	_pos += _speed * deltaTime;

	// 충돌
	const vector<Object*>& objects = GET_SINGLE(ObjectManager)->GetObjects();
	for (Object* obj : objects)
	{
		if (obj->GetObjectType() != ObjectType::Player)
			continue;
		if (obj == _owner)
			continue;

		Vector dir = _pos - obj->GetPos();
		if (dir.Length() < _radius + obj->GetRadius())
		{
			FortressScene* scene = dynamic_cast<FortressScene*>(GET_SINGLE(SceneManager)->GetCurrentScene());
			if (scene)
				scene->ChangePlayerTurn();

			GET_SINGLE(ObjectManager)->Remove(this);
			return;
		}
	}

	// 소멸
	if (_pos.y > GWinSizeY * 1.5 || _pos.y < -GWinSizeY * 1.5)
	{
		FortressScene* scene = dynamic_cast<FortressScene*>(GET_SINGLE(SceneManager)->GetCurrentScene());
		if (scene)
			scene->ChangePlayerTurn();

		GET_SINGLE(ObjectManager)->Remove(this);
		return;
	}
}

void Bullet::Render(HDC hdc)
{
	Utils::DrawCircle(hdc, _pos, static_cast<int32>(_radius));
}
