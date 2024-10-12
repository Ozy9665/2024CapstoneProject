#pragma once
#include "GameObject.h"

/*-----------
	Server
-------------*/

class Player : public GameObject
{
	using Super = GameObject;

public:
	Player();
	virtual ~Player();

public:
	GameSessionRef session;
};

