#pragma once
#include "Component.h"

class PlayerMoveScript : public Component
{
public:
	virtual void Start() override;
	virtual void Update() override;
	virtual void Render(HDC hdc) override;
};

