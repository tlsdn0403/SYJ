#include "pch.h"
#include "ObjectUtils.h"
#include "Player.h"
#include "GameSession.h"
#include "Monster.h"

atomic<int64> ObjectUtils::s_idGenerator = 1;

PlayerRef ObjectUtils::CreatePlayer(GameSessionRef session)
{
	// ID 생성기
	const int64 newId = s_idGenerator.fetch_add(1);

	PlayerRef player = make_shared<Player>();
	player->objectInfo->set_object_id(newId);
	player->posInfo->set_object_id(newId);
	if (session)
	{
		player->nickname = session->nickname;
		player->objectInfo->set_nickname(player->nickname);
	}

	player->session = session;
	session->player.store(player);

	return player;
}

MonsterRef ObjectUtils::CreateMonster(uint64 objectId)
{
	MonsterRef monster = make_shared<Monster>();
	monster->objectInfo->set_object_id(objectId);
	monster->posInfo->set_object_id(objectId);

	return monster;
}
