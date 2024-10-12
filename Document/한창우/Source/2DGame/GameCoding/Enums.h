#pragma once

enum class SceneType
{
	None,
	DevScene,
	GameScene,
	EditScene,

	MenuScene,
	FortressScene,
};

enum class PlayerType
{
	CanonTank,
	MissileTank,
};

enum LAYER_TYPE
{
	LAYER_BACKGROUND,
	LAYER_OBJECT,
	LAYER_EFFECT,
	//...
	LAYER_UI,

	LAYER_MAXCOUNT
};

enum class ColliderType
{
	Box,
	Sphere,
};

enum COLLISION_LAYER_TYPE
{
	CLT_OBJECT,
	CLT_GROUND,
	CLT_WALL,
	// ..
};

enum Dir
{
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT,
};

enum class ObjectState
{
	Idle,
	Move,
	Skill
};

enum class WeaponType
{
	Sword,
	Bow,
	Staff,
};