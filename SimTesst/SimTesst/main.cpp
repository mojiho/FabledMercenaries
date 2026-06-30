#include <cstdio>
#include <memory>
#include <windows.h>
#include "CombatSim.h"
#include "AIBrain.h"

int main()
{
	SetConsoleOutputCP(CP_UTF8);
	CombatSim sim;

	// 유닛+자기 지휘관 추가 헬퍼 (각자 cmdGauge 풀, MP 풀)
	auto add = [&](uint64_t id, Faction f, Class c, Vec3 p, std::unique_ptr<Brain> b)
		{
			sim.AddCommander(id).cmdGauge = 3.f;
			Unit& u = sim.AddUnit(id, id, f, c, p, std::move(b));
			u.mp = u.mpMax;
		};

	// 내 팀: 탱커(앞) + 마법사/궁수(뒤)
	add(1, Faction::Player, Class::Tanker, Vec3{ 100,   0, 0 }, std::make_unique<TankBrain>());
	add(2, Faction::Player, Class::Mage, Vec3{ 0,  40, 0 }, std::make_unique<MageBrain>());
	add(3, Faction::Player, Class::Archer, Vec3{ 0, -40, 0 }, std::make_unique<KiteBrain>());
	add(4, Faction::Player, Class::Healer, Vec3{ -60,   0, 0 }, std::make_unique<HealerBrain>());

	// 적 팀: 전사(돌진) + 암살자 + 궁수
	add(11, Faction::Hostile, Class::Warrior, Vec3{ 800,   0, 0 }, std::make_unique<ChaseAttackBrain>());
	add(12, Faction::Hostile, Class::Assassin, Vec3{ 800,  40, 0 }, std::make_unique<ChaseAttackBrain>());
	add(13, Faction::Hostile, Class::Archer, Vec3{ 850, -40, 0 }, std::make_unique<KiteBrain>());

	sim.OnDeath = [](uint64_t u) { std::printf("    [DEATH] u=%llu\n", (unsigned long long)u); };

	auto hp = [&](uint64_t id) { const Unit* u = sim.GetUnit(id); return u ? u->hp : 0.f; };

	for (int i = 0; i < 400; ++i)
	{
		sim.Tick(1.0f / 20.f);
		if ((i % 10) == 9)
			std::printf("t=%5.1f | 내팀 T%3.0f M%3.0f A%3.0f H% 3.0f | 적팀 W%3.0f S%3.0f A%3.0f\n",
				(i + 1) * 0.05f, hp(1), hp(2), hp(3), hp(4), hp(11), hp(12), hp(13));
	}
	std::getchar();
	return 0;
}