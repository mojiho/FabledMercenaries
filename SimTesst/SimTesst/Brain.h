#pragma once

class CombatSim;	// 전방 선언: Think가 참조(&)만 받음 → 정의 불필요(순환 방지)
class Unit;			// 전방 선언: Brain이 참조(&)만 받음 → 정의 불필요(순환 방지)

// <summary>
// Brain: Unit의 AI/행동 결정 모듈. Unit과 분리하여 테스트 용이.
// </summary>

class Brain
{
public:
	virtual ~Brain() = default;	// 베이스 포인터로 다룸, 다형적 삭제 → 반드시 virtual
	virtual void Decide(CombatSim& sim, Unit& self, float dt) = 0;	// 스스로 IssueCommand
};