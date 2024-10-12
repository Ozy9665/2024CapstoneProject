#include "pch.h"
#include "SpriteRenderer.h"
#include "Sprite.h"
#include "GameObject.h"

void SpriteRenderer::Start()
{

}

void SpriteRenderer::Update()
{

}

void SpriteRenderer::Render(HDC hdc)
{
	if (_sprite == nullptr)
		return;

	Vec2Int size = _sprite->GetPos();
	Vec2 pos = GetOwner()->GetPos();

	::BitBlt(hdc,
		pos.x,
		pos.y,
		GWinSizeX,
		GWinSizeY,
		_sprite->GetDC(),
		_sprite->GetPos().x,
		_sprite->GetPos().y,
		SRCCOPY);
}
