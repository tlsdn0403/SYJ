#include "pch.h"
#include "Monster.h"

Monster::Monster()
{
	_isMonster = true;
}

Monster::~Monster()
{
	delete objectInfo;
}
