#pragma once
#include "Component.h"

class Sprite;

class SpriteRenderer : public Component
{
public:
	virtual void Start();
	virtual void Update();
	virtual void Render(HDC hdc);

	void SetSprite(Sprite* sprite) { _sprite = sprite; }

private:
	Sprite* _sprite = nullptr;
};

