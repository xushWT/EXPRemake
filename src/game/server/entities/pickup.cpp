/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <generated/server_data.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/gamemodes/exp/environment.h>
#include <game/server/gamemodes/exp/exp.h>

#include "character.h"
#include "pickup.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, PickupPhysSize)
{
	m_FromDrop = false;
	m_DieTimer = (float)Server()->Tick();
	m_IsBossShield = false;

	m_Type = Type;

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	if(m_FromDrop)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;
}

void CPickup::Tick()
{
	if(m_IsBossShield)
		return;

	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			if(m_Type == PICKUP_GRENADE || m_Type == PICKUP_SHOTGUN || m_Type == PICKUP_LASER)
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}

	if(m_FromDrop)
	{
		if((float)Server()->Tick() > m_DieTimer + GameServer()->Tuning()->m_PickupLifetime*Server()->TickSpeed())
		{
			GameWorld()->DestroyEntity(this);
			return;
		}
	}

	// Check if a player intersected us
	CCharacter *pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 20.0f, 0);
	if(pChr && pChr->IsAlive())
	{
		CPlayer *pPlayer = pChr->GetPlayer();

		// player picked us up, is someone was hooking us, let them go
		bool Picked = false;
		switch (m_Type)
		{
			case PICKUP_HEALTH:
				if(pChr->IncreaseHealth(4))
				{
					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
				}
				break;

			case PICKUP_ARMOR:
				if(!pPlayer->IsBot() && pPlayer->m_GameExp.m_ArmorMax < 10)
				{
					if(pPlayer->m_GameExp.m_ArmorMax == 0)
						GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: ARMOR. Say /items for more info.");
					else
						GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: ARMOR.");
					
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
					pPlayer->m_GameExp.m_ArmorMax += 1;
					pChr->m_Armor += 1;
					Picked = true;
				}
				break;

			case PICKUP_GRENADE:
				if(pChr->GiveWeapon(WEAPON_GRENADE, g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Maxammo) || !pPlayer->IsBot())
				{
					char aMsg[64];
					str_format(aMsg, sizeof(aMsg), "Picked up: %s.", GetWeaponName(m_Type));
					GameServer()->SendChatTarget(pPlayer->GetCID(), aMsg);
					
					pPlayer->GetWeapon(WEAPON_GRENADE);

					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);
					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_GRENADE);
				}
				break;
			case PICKUP_SHOTGUN:
				if(pChr->GiveWeapon(WEAPON_SHOTGUN, g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Maxammo) || !pPlayer->IsBot())
				{
					char aMsg[64];
					str_format(aMsg, sizeof(aMsg), "Picked up: %s.", GetWeaponName(m_Type));
					GameServer()->SendChatTarget(pPlayer->GetCID(), aMsg);
					
					pPlayer->GetWeapon(WEAPON_SHOTGUN);

					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_SHOTGUN);
				}
				break;
			case PICKUP_LASER:
				if(pChr->GiveWeapon(WEAPON_LASER, g_pData->m_Weapons.m_aId[WEAPON_LASER].m_Maxammo) || !pPlayer->IsBot())
				{
					char aMsg[64];
					str_format(aMsg, sizeof(aMsg), "Picked up: %s.", GetWeaponName(m_Type));
					GameServer()->SendChatTarget(pPlayer->GetCID(), aMsg);
					
					pPlayer->GetWeapon(WEAPON_LASER);

					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_LASER);
				}
				break;

			case PICKUP_NINJA:
				{
					Picked = true;
					// activate ninja on target player
					pChr->GiveNinja();

					// loop through all players, setting their emotes
					CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
					for(; pC; pC = (CCharacter *)pC->TypeNext())
					{
						if (pC != pChr)
							pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
					}

					pChr->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
					break;
				}

			case PICKUP_LIFE:
				{
					if(!pPlayer->IsBot())
					{
						if(pPlayer->m_GameExp.m_Items.m_Lives == 0)
							GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: LIFE. Say /items for more info.");
						else
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "Picked up: LIFE (%d)", pPlayer->m_GameExp.m_Items.m_Lives+1);
							GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
						}
						pPlayer->m_GameExp.m_Items.m_Lives++;
						
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
						Picked = true;
					}
				}
				break;
			
			case PICKUP_MINOR_POTION:
				{
					if(!pPlayer->IsBot())
					{
						if(pPlayer->m_GameExp.m_Items.m_MinorPotions == 0)
							GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: MINOR POTION. Say /items for more info.");
						else
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "Picked up: MINOR POTION (%d)", pPlayer->m_GameExp.m_Items.m_MinorPotions+1);
							GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
						}
						pPlayer->m_GameExp.m_Items.m_MinorPotions++;
						
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
						Picked = true;
					}
				}
				break;
			
			case PICKUP_GREATER_POTION:
				{
					if(!pPlayer->IsBot())
					{
						if(pPlayer->m_GameExp.m_Items.m_GreaterPotions == 0)
							GameServer()->SendChatTarget(pPlayer->GetCID(), "Picked up: GREATER POTION. Say /items for more info.");
						else
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "Picked up: GREATER POTION (%d)", pPlayer->m_GameExp.m_Items.m_GreaterPotions+1);
							GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
						}
						pPlayer->m_GameExp.m_Items.m_GreaterPotions++;
						
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
						Picked = true;
					}
				}
				break;

			default:
				break;
		};

		if(Picked)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d/%d",
				pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
			if(m_FromDrop)
			{
				GameServer()->m_World.DestroyEntity(this);
			}
			else
			{
				int RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				if(RespawnTime >= 0)
					m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
			}
		}
	}

	if(m_Type == PICKUP_MINOR_POTION || m_Type == PICKUP_GREATER_POTION)
	{
		if((float)Server()->Tick() > m_AnimationTimer)
		{
			int ID = -1;
			for(int i = 0; i < g_Config.m_SvMaxClients; i++)
			{
				if(GameServer()->m_apPlayers[i])
				{
					ID = i;
					break;
				}
			}
			if(ID == -1) return;
			GameServer()->CreateDeath(m_Pos, ID);
			GameServer()->CreateDeath(m_Pos, -1);
			float Sec = (m_Type == PICKUP_GREATER_POTION ? 0.3 : 0.5);
			m_AnimationTimer = (float)Server()->Tick() + Sec*Server()->TickSpeed();
		}
	}
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = RealPickup(m_Type);
}

void CPickup::CreateRandomFromBot(int lvl)
{
	m_FromDrop = true;
	
	int r = Server()->Tick()%100;
	
	if(lvl == 1)
	{
		if(r < 70)
			m_Type = PICKUP_HEALTH; //70%
		else if(r < 90)
			m_Type = PICKUP_MINOR_POTION; //20%
		else
		{ //10%
			if(r < 96)
				m_Type = PICKUP_SHOTGUN; //6%
			else
				m_Type = PICKUP_GRENADE; //4%
		}
	}
	else if(lvl == 2)
	{
		if(r < 35)
			m_Type = PICKUP_HEALTH; //35%
		else if(r < 60)
			m_Type = PICKUP_LIFE; //25%
		else if(r < 80)
			m_Type = PICKUP_MINOR_POTION; //20%
		else if(r < 95)
			m_Type = PICKUP_GREATER_POTION; //15%
		else
		{ //5%
			m_Type = PICKUP_KAMIKAZE;
		}
	}
	else if(lvl == 3)
	{
		if(r < 25)
			m_Type = PICKUP_HEALTH; //25%
		else if(r < 30)
			m_Type = PICKUP_LIFE; //5%
		else if(r < 55)
			m_Type = PICKUP_MINOR_POTION; //25%
		else if(r < 80)
			m_Type = PICKUP_GREATER_POTION; //25%
		else
		{ //20%
			if(r < 92)
				m_Type = PICKUP_SHOTGUN; //12%
			else
				m_Type = PICKUP_GRENADE; //8%
		}
	}
	else if(lvl == 4)
	{
		m_Type = PICKUP_FREEZER;
	}
	
	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && !GameServer()->m_apPlayers[id]->IsBot())
			Snap(id);
	}
}



void CPickup::CreateRandomFromTurret(int TurretType)
{
	m_FromDrop = true;
	
	int r = Server()->Tick()%100; //r < 75
	
	if(r < 70)
		m_Type = PICKUP_ARMOR; //70%
	else
	{
		if(TurretType == TURRET_TYPE_LASER)
			m_Type = WEAPON_LASER;
		else if(TurretType == TURRET_TYPE_GUN)
			m_Type = WEAPON_GUN;
	}
	
	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && !GameServer()->m_apPlayers[id]->IsBot())
			Snap(id);
	}
}

void CPickup::MakeBossShield()
{
	m_IsBossShield = true;
	for(int id = 0; id < MAX_CLIENTS; id++)
	{
		if(GameServer()->m_apPlayers[id] && !GameServer()->m_apPlayers[id]->IsBot())
			Snap(id);
	}
}

const char *CPickup::GetWeaponName(int wid)
{
	if(wid == WEAPON_HAMMER)
		return "HAMMER";
	else if(wid == WEAPON_GUN)
		return "GUN";
	else if(wid == PICKUP_SHOTGUN)
		return "SHOTGUN";
	else if(wid == PICKUP_GRENADE)
		return "GRENADE";
	else if(wid == PICKUP_LASER)
		return "LASER";
	else if(wid == PICKUP_NINJA)
		return "NINJA";
	else if(wid == PICKUP_KAMIKAZE)
		return "KAMIKAZE";
	else if(wid == PICKUP_FREEZER)
		return "FREEZER";
	return "?";
}

int CPickup::RealPickup(int Type)
{
	if(Type == PICKUP_LIFE)
		Type = PICKUP_HEALTH;
	else if(Type == PICKUP_MINOR_POTION || Type == PICKUP_GREATER_POTION)
		Type = PICKUP_HEALTH;
	if(Type == WEAPON_KAMIKAZE)
		Type = WEAPON_NINJA;
	else if(Type == WEAPON_FREEZER)
		Type = WEAPON_LASER;
	return Type;
}
