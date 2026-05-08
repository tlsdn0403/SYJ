#pragma once

class ObjectUtils
{
public:
	static PlayerRef CreatePlayer(GameSessionRef session);
	static MonsterRef CreateMonster(uint64 objectId);

private:
	static atomic<int64> s_idGenerator;
};

