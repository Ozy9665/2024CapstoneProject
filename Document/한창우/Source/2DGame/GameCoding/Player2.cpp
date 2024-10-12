#include "pch.h"
#include "Player.h"
#include "InputManager.h"
#include "TimeManager.h"
#include "ObjectManager.h"
#include "Missile.h"
#include "ResourceManager.h"
#include "LineMesh.h"
#include "UIManager.h"
#include "Bullet.h"

Player::Player() : Object(ObjectType::Player)
{

}

Player::~Player()
{

}

void Player::Init()
{
	// �����ͽ�Ʈ : Excel, Json
	// �����ͺ��̽� : Oracle, MSSQL
	// TODO : Data
	_stat.hp	= 100;
	_stat.maxHp = 100;
	_stat.speed = 500;

	// ����� << �����ͽ�Ʈ�� �����ص�������
	// ��ƺ�� << ���������� �޶� �����ͺ��̽�
}

void Player::Update()
{
	float deltaTime = GET_SINGLE(TimeManager)->GetDeltaTime();
	// �Ÿ� = �ð� * �ӵ�

	if (_playerTurn == false)
		return;

	UpdateFireAngle();

	if (GET_SINGLE(InputManager)->GetButton(KeyType::A))
	{
		_pos.x -= deltaTime * _stat.speed;
		_dir = Dir::Left;
	}

	if (GET_SINGLE(InputManager)->GetButton(KeyType::D))
	{
		_pos.x += deltaTime * _stat.speed;
		_dir = Dir::Right;
	}

	if (GET_SINGLE(InputManager)->GetButton(KeyType::W))
	{
		_fireAngle = ::clamp(_fireAngle + 50 * deltaTime, 0.f, 75.f);
	}

	if (GET_SINGLE(InputManager)->GetButton(KeyType::S))
	{
		_fireAngle = ::clamp(_fireAngle - 50 * deltaTime, 0.f, 75.f);
	}

	if (GET_SINGLE(InputManager)->GetButton(KeyType::SpaceBar))
	{
		// TODO : �̻��� �߻�
		float percent = GET_SINGLE(UIManager)->GetPowerPercent();
		percent = min(100, percent + 100 * deltaTime);
		GET_SINGLE(UIManager)->SetPowerPercent(percent);
	}

	if (GET_SINGLE(InputManager)->GetButtonUp(KeyType::SpaceBar))
	{
		// ����
		_playerTurn = false;

		float percent = GET_SINGLE(UIManager)->GetPowerPercent();
		float speed = 10.f * percent;
		float angle = GET_SINGLE(UIManager)->GetBarrelAngle();

		// TODO
		Bullet* bullet = GET_SINGLE(ObjectManager)->CreateObject<Bullet>();
		bullet->SetOwner(this);
		bullet->SetPos(_pos);
		bullet->SetSpeed(Vector{ speed * ::cos(angle * PI / 180), -1 * speed * ::sin(angle * PI / 180)});
		GET_SINGLE(ObjectManager)->Add(bullet);
	}
}

void Player::Render(HDC hdc)
{
	HPEN pen = ::CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
	HPEN oldPen = (HPEN)::SelectObject(hdc, pen);

	if (_playerTurn)
	{
		RECT rect;
		rect.bottom = static_cast<LONG>(_pos.y - 60);
		rect.left = static_cast<LONG>(_pos.x - 10);
		rect.right = static_cast<LONG>(_pos.x + 10);
		rect.top = static_cast<LONG>(_pos.y	- 80);

		HBRUSH brush = ::CreateSolidBrush(RGB(250, 236, 197));
		HBRUSH oldBrush = (HBRUSH)::SelectObject(hdc, brush);

		::Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);

		::SelectObject(hdc, oldBrush);
		::DeleteObject(brush);
	}

	::SelectObject(hdc, oldPen);
	::DeleteObject(pen);
}

wstring Player::GetMeshKey()
{
	if (_playerType == PlayerType::MissileTank)
		return L"MissileTank";

	return L"CanonTank";
}

void Player::UpdateFireAngle()
{
	if (_dir == Dir::Left)
	{
		GET_SINGLE(UIManager)->SetPlayerAngle(180);
		GET_SINGLE(UIManager)->SetBarrelAngle(180 - _fireAngle);
	}
	else
	{
		GET_SINGLE(UIManager)->SetPlayerAngle(0);
		GET_SINGLE(UIManager)->SetBarrelAngle(_fireAngle);
	}
}
