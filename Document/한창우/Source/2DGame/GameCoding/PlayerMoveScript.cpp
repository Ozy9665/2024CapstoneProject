#include "pch.h"
#include "PlayerMoveScript.h"
#include "InputManager.h"
#include "TimeManager.h"
#include "GameObject.h"

void PlayerMoveScript::Start()
{

}

void PlayerMoveScript::Update()
{
	float deltaTime = GET_SINGLE(TimeManager)->GetDeltaTime();

	// TODO
	if (GET_SINGLE(InputManager)->GetButton(KeyType::W))
	{
		Vec2 pos = GetOwner()->GetPos();
		pos.y -= 100 * deltaTime;
		GetOwner()->SetPos(pos);
	}
}

void PlayerMoveScript::Render(HDC hdc)
{

}
