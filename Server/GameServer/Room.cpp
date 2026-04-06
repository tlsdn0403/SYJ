#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "GameSession.h"
#include "Monster.h"
#include "ObjectUtils.h"

RoomRef GRoom = make_shared<Room>();

Room::Room()
{

}

Room::~Room()
{

}

bool Room::EnterRoom(ObjectRef object, bool randPos /*= true*/)
{
	bool success = AddObject(object);

	// 랜덤 위치
	if (randPos)
	{
		object->posInfo->set_x(Utils::GetRandom(0.f, 500.f));
		object->posInfo->set_y(Utils::GetRandom(0.f, 500.f));
		object->posInfo->set_z(100.f);
		object->posInfo->set_yaw(Utils::GetRandom(0.f, 100.f));
	}

	// 입장 사실을 신입 플레이어에게 알린다
	if (auto player = dynamic_pointer_cast<Player>(object))
	{
		Protocol::S_ENTER_GAME enterGamePkt;
		enterGamePkt.set_success(success);
		
		Protocol::ObjectInfo* playerInfo = new Protocol::ObjectInfo();
		playerInfo->CopyFrom(*object->objectInfo);
		enterGamePkt.set_allocated_player(playerInfo);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	// 입장 사실을 다른 플레이어에게 알린다
	{
		Protocol::S_SPAWN spawnPkt;

		Protocol::ObjectInfo* objectInfo = spawnPkt.add_players();
		objectInfo->CopyFrom(*object->objectInfo);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(spawnPkt);
		Broadcast(sendBuffer, object->objectInfo->object_id());
	}

	// 기존 입장한 플레이어 목록을 신입 플레이어한테 전송해준다
	if (auto player = dynamic_pointer_cast<Player>(object))
	{
		Protocol::S_SPAWN spawnPkt;

		for (auto& item : _objects)
		{
			if (item.second->IsPlayer() == false)
				continue;

			if (item.second == object)
				continue;

			Protocol::ObjectInfo* playerInfo = spawnPkt.add_players();
			playerInfo->CopyFrom(*item.second->objectInfo);
		}

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(spawnPkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	return success;
}

bool Room::LeaveRoom(ObjectRef object)
{
	if (object == nullptr)
		return false;

	const uint64 objectId = object->objectInfo->object_id();
	bool success = RemoveObject(objectId);

	// 퇴장 사실을 퇴장하는 플레이어에게 알린다
	if (auto player = dynamic_pointer_cast<Player>(object))
	{
		Protocol::S_LEAVE_GAME leaveGamePkt;

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(leaveGamePkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	// 퇴장 사실을 알린다
	{
		Protocol::S_DESPAWN despawnPkt;
		despawnPkt.add_object_ids(objectId);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(despawnPkt);
		Broadcast(sendBuffer, objectId);

		if (auto player = dynamic_pointer_cast<Player>(object))
			if (auto session = player->session.lock())
				session->Send(sendBuffer);
	}

	return success;
}

bool Room::HandleEnterPlayer(PlayerRef player)
{
	bool success = EnterRoom(player, true);

	if (success == false)
		return false;

	Protocol::S_SPAWN_ITEM spawnItemPkt;
	Protocol::ObjectInfo* itemInfo = spawnItemPkt.add_items();

	itemInfo->set_object_id(1); // 이 무기의 고유 번호는 1번!
	itemInfo->set_object_type(Protocol::OBJECT_TYPE_ITEM);
	itemInfo->set_weapon_type(Protocol::WEAPON_TYPE_RIFLE);

	Protocol::PosInfo* pos = itemInfo->mutable_pos_info();
	pos->set_x(-860.0f);
	pos->set_y(-180.0f);
	pos->set_z(30.0f);
	pos->set_yaw(0.0f);

	// 방금 접속한 플레이어에게 패킷 전송
	SendBufferRef itemBuffer = ServerPacketHandler::MakeSendBuffer(spawnItemPkt);
	player->session.lock()->Send(itemBuffer);

	return true;
}

bool Room::HandleLeavePlayer(PlayerRef player)
{
	if (player == nullptr) return false;

	// 나가는 유저의 ID를 미리 기억해둠
	uint64 leaveId = player->objectInfo->object_id();

	// 기존에 만들어둔 방 퇴장 로직 실행 (서버 내부 장부에서 지우는 역할)
	bool success = LeaveRoom(player);

	// 퇴장에 실패했거나 이미 나간 유저라면 여기서 끝냄
	if (success == false) return false;

	// 방에 남아있는 다른 사람들에게 "얘 나갔다"고 소문내기!
	Protocol::S_LEAVE_GAME leavePkt;
	leavePkt.set_object_id(leaveId);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(leavePkt);
	Broadcast(sendBuffer);

	cout << "[Server] " << leaveId << "번 유저 접속 종료 및 맵 퇴장 알림 완료!" << endl;

	return true;
}

void Room::HandleMove(Protocol::C_MOVE pkt)
{
	const uint64 objectId = pkt.info().object_id();
	if (_objects.find(objectId) == _objects.end())
		return;

	// 적용
	PlayerRef player = dynamic_pointer_cast<Player>(_objects[objectId]);
	player->posInfo->CopyFrom(pkt.info());

	// 이동 사실을 알린다 (본인 포함? 빼고?)
	{
		Protocol::S_MOVE movePkt;
		{
			Protocol::PosInfo* info = movePkt.mutable_info();
			info->CopyFrom(pkt.info());
		}
		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
		Broadcast(sendBuffer);
	}
}

void Room::HandleEquipWeapon(PlayerRef player, Protocol::C_EQUIP_WEAPON pkt)
{
	if (player == nullptr)
		return;

	// 이 플레이어의 구조체 정보 갱신
	// (참고: 지금은 무조건 라이플을 주웠다고 가정하고 하드코딩합니다. 나중에는 맵에 떨어진 아이템 ID를 조회해서 타입을 찾아야 합니다.)
	player->objectInfo->set_weapon_type(Protocol::WEAPON_TYPE_RIFLE);

	// 다른 사람들에게 뿌릴 S_EQUIP_WEAPON 패킷 조립
	Protocol::S_EQUIP_WEAPON equipPkt;
	equipPkt.set_playerid(player->objectInfo->object_id()); // 누가 주웠는지 (본인)
	equipPkt.set_itemobjectid(pkt.itemobjectid());          // 어떤 아이템을 주웠는지 (클라가 보내준 맵의 총기 ID)
	equipPkt.set_weapontype(Protocol::WEAPON_TYPE_RIFLE);   // 무슨 타입인지

	// 방에 있는 모든 사람에게 소문내기 (Broadcast)
	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(equipPkt);
	Broadcast(sendBuffer);
}

void Room::UpdateTick()
{
	//cout << "Update Room" << endl;

	DoTimer(100, &Room::UpdateTick);
}

RoomRef Room::GetRoomRef()
{
	return static_pointer_cast<Room>(shared_from_this());
}

bool Room::AddObject(ObjectRef object)
{
	// 있다면 문제가 있다.
	if (_objects.find(object->objectInfo->object_id()) != _objects.end())
		return false;

	_objects.insert(make_pair(object->objectInfo->object_id(), object));

	object->room.store(GetRoomRef());

	return true;
}

bool Room::RemoveObject(uint64 objectId)
{
	// 없다면 문제가 있다.
	if (_objects.find(objectId) == _objects.end())
		return false;

	ObjectRef object = _objects[objectId];
	PlayerRef player = dynamic_pointer_cast<Player>(object);
	if (player)
		player->room.store(weak_ptr<Room>());

	_objects.erase(objectId);

	return true;
}

void Room::Broadcast(SendBufferRef sendBuffer, uint64 exceptId)
{
	for (auto& item : _objects)
	{
		PlayerRef player = dynamic_pointer_cast<Player>(item.second);
		if (player == nullptr)
			continue;
		if (player->objectInfo->object_id() == exceptId)
			continue;

		if (GameSessionRef session = player->session.lock())
			session->Send(sendBuffer);
	}
}