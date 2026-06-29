#include <cstdio>
#include <memory>
#include "CombatSim.h"
#include "AIBrain.h"
#include <windows.h>

int main()
{
	SetConsoleOutputCP(CP_UTF8);   // main 첫 줄
	CombatSim sim;
	const uint64_t p1 = 1, p2 = 2;
	const uint64_t w = 10, a = 11, s = 12;     // 내 용병 3기
	const uint64_t e1 = 20, e2 = 21;           // 적 2기

	Commander& cmd = sim.AddCommander(p1);     // 내 지휘관(게이지 관찰용으로 ref 보관)
	sim.AddCommander(p2);                      // 적 지휘관(적 brain의 명령 발행에 필요)

	// 내 파티 — 전부 ownerId=p1 → cmdGauge '공유'(한 지휘관의 명령 대역폭)
	sim.AddUnit(w, p1, Faction::Player, Class::Warrior, Vec3{ 0,   0, 0 });
	sim.AddUnit(a, p1, Faction::Player, Class::Archer, Vec3{ 0, -40, 0 });
	sim.AddUnit(s, p1, Faction::Player, Class::Assassin, Vec3{ 0,  40, 0 });

	// 적 — AI(ChaseAttackBrain)가 스스로 가까운 적 공격
	sim.AddUnit(e1, p2, Faction::Hostile, Class::Tanker, Vec3{ 100,  0, 0 }, std::make_unique<ChaseAttackBrain>());
	sim.AddUnit(e2, p2, Faction::Hostile, Class::Mage, Vec3{ 100, 50, 0 }, std::make_unique<ChaseAttackBrain>());

	sim.OnDeath = [](uint64_t u) { std::printf("    [DEATH] u=%llu\n", (unsigned long long)u); };

	auto atk = [](uint64_t t) { Command c; c.type = CommandType::Attack; c.targetId = t; return c; };

	// === ① cmdGauge 게이팅 시연 ===
	cmd.cmdGauge = 2.0f;   // 일부러 2만 채움 → 명령 2번까지만 가능
	std::printf("[명령] gauge=%.1f\n", cmd.cmdGauge);
	std::printf("  w->e1 = %d\n", (int)sim.IssueCommand(w, atk(e1), false));  // 0 Accepted (gauge 2->1)
	std::printf("  a->e1 = %d\n", (int)sim.IssueCommand(a, atk(e1), false));  // 0 Accepted (1->0)
	std::printf("  s->e2 = %d  <- 대역폭 소진! (gauge=%.1f)\n",
		(int)sim.IssueCommand(s, atk(e2), false), cmd.cmdGauge);             // 1 Rejected (0<1)

	// === ② 틱 루프 ===
	bool reissued = false;
	for (int i = 0; i < 200; ++i)
	{
		sim.Tick(1.0f / 20.f);

		// 게이지 회복되면 아까 막힌 암살자(s)에게 재명령 한 번
		if (!reissued && cmd.cmdGauge >= 1.0f)
		{
			std::printf(">> t=%4.2f 회복! s->e2 재명령 = %d (gauge=%.1f)\n",
				(i + 1) * 0.05f, (int)sim.IssueCommand(s, atk(e2), false), cmd.cmdGauge);
			reissued = true;
		}

		if ((i % 10) == 9)   // 0.5초마다 요약
		{
			const Unit* W = sim.GetUnit(w);  const Unit* A = sim.GetUnit(a);  const Unit* S = sim.GetUnit(s);
			const Unit* E1 = sim.GetUnit(e1); const Unit* E2 = sim.GetUnit(e2);
			std::printf("t=%4.1f g=%.1f | W %3.0f | A %3.0f | S %3.0f | e1 %3.0f | e2 %3.0f\n",
				(i + 1) * 0.05f, cmd.cmdGauge, W->hp, A->hp, S->hp, E1->hp, E2->hp);
		}
	}
	std::getchar();
	return 0;
}