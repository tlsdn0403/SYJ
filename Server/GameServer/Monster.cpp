#include "pch.h"
#include "Monster.h"

Monster::Monster()
{
	_isMonster = true;
}

Monster::~Monster()
{
}


void Monster::SetMaxHp(float maxHp)
{
	_maxHp = (maxHp > 0.0f) ? maxHp : 1.0f;
	if (_hp > _maxHp)
	{
		_hp = _maxHp;
	}
}

void Monster::ApplyDamage(float damage)
{
	if (damage <= 0.0f || IsDead())
	{
		return;
	}

	_hp -= damage;
	if (_hp < 0.0f)
	{
		_hp = 0.0f;
	}
}

void Monster::TickCooldown(float deltaSeconds)
{
	if (_attackCooldownRemaining <= 0.0f)
	{
		return;
	}

	_attackCooldownRemaining -= deltaSeconds;
	if (_attackCooldownRemaining < 0.0f)
	{
		_attackCooldownRemaining = 0.0f;
	}
}

void Monster::StartAttackCooldown(float cooldownSeconds)
{
	_attackCooldownRemaining = (cooldownSeconds > 0.0f) ? cooldownSeconds : 0.0f;
}
