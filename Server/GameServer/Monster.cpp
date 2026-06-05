#include "pch.h"
#include "Monster.h"

Monster::Monster()
{
	_isMonster = true;
	InitializeBoneDurability();
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

bool Monster::ApplyBoneDamage(const std::string& hitBoneName, float damage, std::string& outBrokenBoneName)
{
	outBrokenBoneName.clear();
	if (hitBoneName.empty() || damage <= 0.0f || IsDead())
	{
		return false;
	}

	const std::string targetBoneName = GetParentBoneForDamage(hitBoneName);
	if (targetBoneName.empty() || _brokenBones.find(targetBoneName) != _brokenBones.end())
	{
		return false;
	}

	auto durabilityIt = _boneDurability.find(targetBoneName);
	if (durabilityIt == _boneDurability.end())
	{
		return false;
	}

	durabilityIt->second -= damage;
	if (durabilityIt->second > 0.0f)
	{
		return false;
	}

	_brokenBones.insert(targetBoneName);
	outBrokenBoneName = targetBoneName;
	if (IsLegBone(targetBoneName))
	{
		_moveSpeedScale = 0.55f;
	}

	return true;
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

void Monster::InitializeBoneDurability()
{
	_boneDurability["head"] = 10.0f;
	_boneDurability["upperarm_l"] = 15.0f;
	_boneDurability["lowerarm_l"] = 10.0f;
	_boneDurability["upperarm_r"] = 15.0f;
	_boneDurability["lowerarm_r"] = 10.0f;
	_boneDurability["thigh_l"] = 20.0f;
	_boneDurability["calf_l"] = 15.0f;
	_boneDurability["thigh_r"] = 20.0f;
	_boneDurability["calf_r"] = 15.0f;
	_boneDurability["spine_01"] = 50.0f;
}

std::string Monster::GetParentBoneForDamage(const std::string& hitBoneName) const
{
	std::string boneName = hitBoneName;
	for (char& ch : boneName)
	{
		ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
	}

	if (boneName.find("neck") != std::string::npos || boneName.find("head") != std::string::npos)
		return "head";

	if (boneName.find("_l") != std::string::npos)
	{
		if (boneName.find("hand") != std::string::npos ||
			boneName.find("finger") != std::string::npos ||
			boneName.find("thumb") != std::string::npos ||
			boneName.find("index") != std::string::npos ||
			boneName.find("middle") != std::string::npos ||
			boneName.find("pinky") != std::string::npos ||
			boneName.find("ring") != std::string::npos ||
			boneName.find("lowerarm") != std::string::npos ||
			boneName.find("twist") != std::string::npos)
		{
			return "lowerarm_l";
		}
		if (boneName.find("upperarm") != std::string::npos || boneName.find("clavicle") != std::string::npos)
			return "upperarm_l";
		if (boneName.find("foot") != std::string::npos || boneName.find("ball") != std::string::npos || boneName.find("calf") != std::string::npos)
			return "calf_l";
		if (boneName.find("thigh") != std::string::npos)
			return "thigh_l";
	}

	if (boneName.find("_r") != std::string::npos)
	{
		if (boneName.find("hand") != std::string::npos ||
			boneName.find("finger") != std::string::npos ||
			boneName.find("thumb") != std::string::npos ||
			boneName.find("index") != std::string::npos ||
			boneName.find("middle") != std::string::npos ||
			boneName.find("pinky") != std::string::npos ||
			boneName.find("ring") != std::string::npos ||
			boneName.find("lowerarm") != std::string::npos ||
			boneName.find("twist") != std::string::npos)
		{
			return "lowerarm_r";
		}
		if (boneName.find("upperarm") != std::string::npos || boneName.find("clavicle") != std::string::npos)
			return "upperarm_r";
		if (boneName.find("foot") != std::string::npos || boneName.find("ball") != std::string::npos || boneName.find("calf") != std::string::npos)
			return "calf_r";
		if (boneName.find("thigh") != std::string::npos)
			return "thigh_r";
	}

	if (boneName.find("spine") != std::string::npos || boneName.find("pelvis") != std::string::npos)
		return "spine_01";

	return boneName;
}

bool Monster::IsLegBone(const std::string& boneName) const
{
	return boneName == "thigh_l" ||
		boneName == "thigh_r" ||
		boneName == "calf_l" ||
		boneName == "calf_r";
}