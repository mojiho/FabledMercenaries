#include <cstdio>
#include "CombatSim.h"

const char* StateName(ActionState s)
{
	switch (s) {
	case ActionState::Idle:          return "Idle";
	case ActionState::Moving:        return "Moving";
	case ActionState::AttackWindup:  return "Windup";
	case ActionState::AttackRecover: return "Recover";
	case ActionState::Casting:       return "Casting";
	case ActionState::Stunned:       return "Stunned";
	case ActionState::Dead:          return "Dead";
	}
	return "?";
}

int main()
{
	CombatSim sim;
	const uint64_t p = 1, hero = 100, foe = 200;
	Commander& c = sim.AddCommander(p); c.cmdGauge = c.cmdMax;

	sim.AddUnit(hero, p, Faction::Player, Class::Warrior, Vec3{ 0,0,0 });
	sim.AddUnit(foe, p, Faction::Hostile, Class::Tanker, Vec3{ 50,0,0 });

	sim.OnAttackFired = [](uint64_t u) { std::printf("    [FIRE]    u=%llu\n", (unsigned long long)u); };
	sim.OnDamaged = [](uint64_t u, bool back, bool crit) { std::printf("    [DAMAGED] u=%llu back=%d crit=%d\n", (unsigned long long)u, back, crit); };
	sim.OnDeath = [](uint64_t u) { std::printf("    [DEATH]   u=%llu\n", (unsigned long long)u); };
	sim.OnSkillCast = [](uint64_t u, SkillType t) { std::printf("    [SKILL]   u=%llu\n", (unsigned long long)u); };

	Command atk; atk.type = CommandType::Attack; atk.targetId = foe;
	sim.IssueCommand(hero, atk, false);

	for (int i = 0; i < 120; ++i)
	{
		sim.Tick(1.0f / 20.f);
		const Unit* h = sim.GetUnit(hero);
		const Unit* f = sim.GetUnit(foe);
		std::printf("t=%5.2f hero=%-7s foeHP=%3.0f foe=%-7s\n",
			(i + 1) * 0.05f, StateName(h->GetActionState()), f->hp, StateName(f->GetActionState()));
	}
	std::getchar();
	return 0;
}