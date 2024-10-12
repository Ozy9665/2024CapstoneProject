#pragma once
#include "SoundManager.h"
#include "Sound.h"

class Actor;
class UI;
class Creature;

class Scene
{
public:
	Scene();
	virtual ~Scene();

	virtual void Init();
	virtual void Update();
	virtual void Render(HDC hdc);

public:
	virtual void AddActor(Actor* actor);
	virtual void RemoveActor(Actor* actor);

	Creature* GetCreatureAt(Vec2Int cellPos);

	void Clear();

protected:
	vector<Actor*>	_actors[LAYER_MAXCOUNT];
	vector<UI*>		_uis;
};

