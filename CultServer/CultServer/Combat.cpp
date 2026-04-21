#include "Combat.h"
#include <concurrent_unordered_map.h>

#include "map.h"
#include "MapManager.h"
#include "CultistAI.h"

extern concurrency::concurrent_unordered_map<int, std::shared_ptr<SESSION>> g_users;
extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;

bool line_sphere_intersect(
	const FVector& start, const FVector& end,
	const FVector& center, double radius)
{
	FVector d{
		end.x - start.x,
		end.y - start.y,
		end.z - start.z
	};

	FVector f{
		start.x - center.x,
		start.y - center.y,
		start.z - center.z
	};

	double a = d.x * d.x + d.y * d.y + d.z * d.z;
	double b = 2.0 * (f.x * d.x + f.y * d.y + f.z * d.z);
	double c = (f.x * f.x + f.y * f.y + f.z * f.z) - (radius * radius);

	double discriminant = b * b - 4.0 * a * c;

	if (discriminant < 0.0)
		return false; // 충돌 없음

	discriminant = std::sqrt(discriminant);

	double t1 = (-b - discriminant) / (2.0 * a);
	double t2 = (-b + discriminant) / (2.0 * a);

	return (t1 >= 0.0 && t1 <= 1.0) ||
		(t2 >= 0.0 && t2 <= 1.0);
}

void baton_sweep(int c_id, HitPacket* p)
{
	auto it = g_users.find(c_id);
	if (it == g_users.end())
		return;

	auto attacker = it->second;
	int room = attacker->room_id;
	if (room < 0)
		return;

	FVector start{ p->TraceStart.x, p->TraceStart.y, p->TraceStart.z };
	FVector forward{ p->TraceDir.x, p->TraceDir.y, p->TraceDir.z };

	Ray ray{
		static_cast<float>(start.x),
		static_cast<float>(start.y),
		static_cast<float>(start.z),
		static_cast<float>(forward.x),
		static_cast<float>(forward.y),
		static_cast<float>(forward.z)
	};

	double range = BATON_RANGE;
	float mapHitDist;
	int mapTri;

	MAP* map = GetMap(room);
	if (map && map->LineTrace(ray, static_cast<float>(range), mapHitDist, mapTri))
	{
		std::cout << "[MAP HIT] dist=" << mapHitDist
			<< " tri=" << mapTri << "\n";

		range = mapHitDist;
	}
	else
	{
		std::cout << "[MAP MISS]\n";
	}

	FVector end = start + forward * range;

	for (int otherId : g_rooms[room].first.player_ids)
	{
		if (otherId == -1 || otherId == c_id)
			continue;

		auto oit = g_users.find(otherId);
		if (oit == g_users.end())
			continue;

		auto& target = oit->second;
		if (!target)
			continue;

		if (target->role == 1)
			continue;

		if (target->role != 100 && !target->isValidSocket())
			continue;

		FVector targetPos{
			target->cultist_state.PositionX,
			target->cultist_state.PositionY,
			target->cultist_state.PositionZ
		};

		if (line_sphere_intersect(start, end, targetPos, 50.0))
		{
			if (target->role == 100)
			{
				Vec3 attackerPos{
					static_cast<float>(start.x),
					static_cast<float>(start.y),
					static_cast<float>(start.z)
				};

				auto* ai = dynamic_cast<CultistAIController*>(target->ai.get());
				if (ai)
				{
					ai->ApplyBatonHit(attackerPos);
				}
			}

			HitResultPacket result{
				hitHeader,
				sizeof(HitResultPacket),
				c_id,
				otherId,
				EWeaponType::Baton
			};

			broadcast_in_room(*attacker, &result, VIEW_RANGE);
			return;
		}
	}
}

void line_trace(int c_id, HitPacket* p)
{
	auto it = g_users.find(c_id);
	if (it == g_users.end())
		return;

	auto attacker = it->second;
	int room = attacker->room_id;
	if (room < 0)
		return;

	FVector start{ p->TraceStart.x, p->TraceStart.y, p->TraceStart.z };
	FVector dir{ p->TraceDir.x,   p->TraceDir.y,   p->TraceDir.z };

	Ray ray{
		static_cast<float>(start.x),
		static_cast<float>(start.y),
		static_cast<float>(start.z),
		static_cast<float>(dir.x),
		static_cast<float>(dir.y),
		static_cast<float>(dir.z)
	};

	double range = (p->Weapon == EWeaponType::Taser) ? TASER_RANGE : PISTOL_RANGE;
	float mapHitDist;
	int mapTri;

	MAP* map = GetMap(room);
	if (map && map->LineTrace(ray, static_cast<float>(range), mapHitDist, mapTri))
	{
		std::cout << "[MAP HIT] dist=" << mapHitDist
			<< " tri=" << mapTri << "\n";

		range = mapHitDist;
	}
	else
	{
		std::cout << "[MAP MISS]\n";
	}

	FVector end = start + dir * range;

	for (int otherId : g_rooms[room].first.player_ids)
	{
		if (otherId == -1 || otherId == c_id)
			continue;

		auto oit = g_users.find(otherId);
		if (oit == g_users.end())
			continue;

		auto& target = oit->second;
		if (!target)
			continue;

		if (target->role == 1)
			continue;

		if (target->role != 100 && !target->isValidSocket())
			continue;

		FVector targetPos{
			target->cultist_state.PositionX,
			target->cultist_state.PositionY,
			target->cultist_state.PositionZ
		};

		if (line_sphere_intersect(start, end, targetPos, 60.0))
		{
			HitResultPacket result{
				hitHeader,
				sizeof(HitResultPacket),
				c_id,
				otherId,
				p->Weapon
			};

			broadcast_in_room(*attacker, &result, VIEW_RANGE);
			return;
		}
	}
}