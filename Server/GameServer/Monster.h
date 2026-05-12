#pragma once
#include "Creature.h"

class Monster : public Creature
{
public:
	Monster();
	virtual ~Monster();

public:
	float GetHp() const { return _hp; }
	float GetMaxHp() const { return _maxHp; }
	bool IsDead() const { return _hp <= 0.0f; }
	bool CanAttack() const { return _attackCooldownRemaining <= 0.0f; }
	void SetMaxHp(float maxHp);
	void ApplyDamage(float damage);
	void TickCooldown(float deltaSeconds);
	void StartAttackCooldown(float cooldownSeconds);

private:
	float _hp = 100.0f;
	float _maxHp = 100.0f;
	float _attackCooldownRemaining = 0.0f;
};
