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
	float GetMoveSpeedScale() const { return _moveSpeedScale; }
	void SetMaxHp(float maxHp);
	void ApplyDamage(float damage);
	bool ApplyBoneDamage(const std::string& hitBoneName, float damage, std::string& outBrokenBoneName);
	void TickCooldown(float deltaSeconds);
	void StartAttackCooldown(float cooldownSeconds);

private:
	void InitializeBoneDurability();
	std::string GetParentBoneForDamage(const std::string& hitBoneName) const;
	bool IsLegBone(const std::string& boneName) const;

	float _hp = 100.0f;
	float _maxHp = 100.0f;
	float _attackCooldownRemaining = 0.0f;
	float _moveSpeedScale = 1.0f;
	unordered_map<std::string, float> _boneDurability;
	unordered_set<std::string> _brokenBones;
};