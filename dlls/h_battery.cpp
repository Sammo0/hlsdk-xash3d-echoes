/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== h_battery.cpp ========================================================

  battery-related code

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "skill.h"
#include "gamerules.h"
#include "effects.h"

class CRecharge : public CBaseToggle
{
public:
	void Spawn();
	void Precache( void );
	void EXPORT Off(void);
	void EXPORT Recharge(void);
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return ( CBaseToggle::ObjectCaps() | FCAP_CONTINUOUS_USE ) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	float m_flNextCharge; 
	int m_iReactivate; // DeathMatch Delay until reactvated
	int m_iJuice;
	int m_iOn;			// 0 = off, 1 = startup, 2 = going
	float m_flSoundTime;
};

TYPEDESCRIPTION CRecharge::m_SaveData[] =
{
	DEFINE_FIELD( CRecharge, m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( CRecharge, m_iReactivate, FIELD_INTEGER ),
	DEFINE_FIELD( CRecharge, m_iJuice, FIELD_INTEGER ),
	DEFINE_FIELD( CRecharge, m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( CRecharge, m_flSoundTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CRecharge, CBaseEntity )

LINK_ENTITY_TO_CLASS( func_recharge, CRecharge )

void CRecharge::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "style" ) ||
		FStrEq( pkvd->szKeyName, "height" ) ||
		FStrEq( pkvd->szKeyName, "value1" ) ||
		FStrEq( pkvd->szKeyName, "value2" ) ||
		FStrEq( pkvd->szKeyName, "value3" ) )
	{
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "dmdelay" ) )
	{
		m_iReactivate = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CRecharge::Spawn()
{
	Precache();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( pev, pev->origin );		// set size and link into world
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	m_iJuice = gSkillData.suitchargerCapacity;
	pev->frame = 0;			
}

void CRecharge::Precache()
{
	PRECACHE_SOUND( "items/suitcharge1.wav" );
	PRECACHE_SOUND( "items/suitchargeno1.wav" );
	PRECACHE_SOUND( "items/suitchargeok1.wav" );
}

void CRecharge::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	// if it's not a player, ignore
	if( !FClassnameIs( pActivator->pev, "player" ) )
		return;

	// if there is no juice left, turn it off
	if( m_iJuice <= 0 )
	{
		pev->frame = 1;			
		Off();
	}

	// if the player doesn't have the suit, or there is no juice left, make the deny noise
	if( ( m_iJuice <= 0 ) || ( !( pActivator->pev->weapons & ( 1 << WEAPON_SUIT ) ) ) )
	{
		if( m_flSoundTime <= gpGlobals->time )
		{
			m_flSoundTime = gpGlobals->time + 0.62;
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/suitchargeno1.wav", 0.85, ATTN_NORM );
		}
		return;
	}

	pev->nextthink = pev->ltime + 0.25;
	SetThink( &CRecharge::Off );

	// Time to recharge yet?
	if( m_flNextCharge >= gpGlobals->time )
		return;

	// Make sure that we have a caller
	if( !pActivator )
		return;

	m_hActivator = pActivator;

	//only recharge the player
	if( !m_hActivator->IsPlayer() )
		return;
	
	// Play the on sound or the looping charging sound
	if( !m_iOn )
	{
		m_iOn++;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/suitchargeok1.wav", 0.85, ATTN_NORM );
		m_flSoundTime = 0.56 + gpGlobals->time;
	}
	if( ( m_iOn == 1 ) && ( m_flSoundTime <= gpGlobals->time ) )
	{
		m_iOn++;
		EMIT_SOUND( ENT( pev ), CHAN_STATIC, "items/suitcharge1.wav", 0.85, ATTN_NORM );
	}

	// charge the player
	if( m_hActivator->pev->armorvalue < 100 )
	{
		m_iJuice--;
		m_hActivator->pev->armorvalue += 1;

		if( m_hActivator->pev->armorvalue > 100 )
			m_hActivator->pev->armorvalue = 100;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + 0.1;
}

void CRecharge::Recharge( void )
{
	m_iJuice = gSkillData.suitchargerCapacity;
	pev->frame = 0;	
	SetThink( &CBaseEntity::SUB_DoNothing );
}

void CRecharge::Off( void )
{
	// Stop looping sound.
	if( m_iOn > 1 )
		STOP_SOUND( ENT( pev ), CHAN_STATIC, "items/suitcharge1.wav" );

	m_iOn = 0;

	if( ( !m_iJuice ) &&  ( ( m_iReactivate = g_pGameRules->FlHEVChargerRechargeTime() ) > 0 ) )
	{
		pev->nextthink = pev->ltime + m_iReactivate;
		SetThink( &CRecharge::Recharge );
	}
	else
		SetThink( &CBaseEntity::SUB_DoNothing );
}

//-------------------------------------------------------------
// Wall mounted health kit (PS2 && Decay)
//-------------------------------------------------------------

class CRechargeGlassDecay : public CBaseAnimating
{
public:
	void Spawn();
};

void CRechargeGlassDecay::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;

	SET_MODEL(ENT(pev), "models/hev_glass.mdl");
	pev->renderamt = 150;
	pev->rendermode = kRenderTransTexture;
}

class CRechargeDecay : public CBaseAnimating
{
public:
	void Spawn();
	void Precache(void);
	void EXPORT SearchForPlayer();
	void EXPORT Off( void );
	void EXPORT Recharge( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return ( CBaseAnimating::ObjectCaps() | FCAP_CONTINUOUS_USE ) & ~FCAP_ACROSS_TRANSITION; }
	void TurnChargeToPlayer(const Vector player);
	void SetChargeState(int state);
	void SetChargeController(float yaw);

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	enum {
		Still,
		Deploy,
		Idle,
		GiveShot,
		Healing,
		RetractShot,
		RetractArm,
		Inactive
	};

	float m_flNextCharge; 
	int m_iJuice;
	int m_iState;
	float m_flSoundTime;
	CRechargeGlassDecay* m_glass;
	BOOL m_playingChargeSound;
	CBeam* m_beam;

protected:
	void SetMySequence(const char* sequence);
	void CreateBeam();
};

TYPEDESCRIPTION CRechargeDecay::m_SaveData[] =
{
	DEFINE_FIELD( CRechargeDecay, m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( CRechargeDecay, m_iJuice, FIELD_INTEGER ),
	DEFINE_FIELD( CRechargeDecay, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CRechargeDecay, m_flSoundTime, FIELD_TIME ),
	DEFINE_FIELD( CRechargeDecay, m_glass, FIELD_CLASSPTR),
	DEFINE_FIELD( CRechargeDecay, m_beam, FIELD_CLASSPTR),
	DEFINE_FIELD( CRechargeDecay, m_playingChargeSound, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE( CRechargeDecay, CBaseAnimating )

void CRechargeDecay::Spawn()
{
	Precache();

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;

	SET_MODEL(ENT(pev), "models/hev.mdl");
	UTIL_SetSize(pev, Vector(-12, -16, 0), Vector(12, 16, 48));
	UTIL_SetOrigin(pev, pev->origin);
	m_iJuice = gSkillData.suitchargerCapacity;
	pev->skin = 0;

	m_glass = GetClassPtr( (CRechargeGlassDecay *)NULL );
	m_glass->Spawn();
	UTIL_SetOrigin( m_glass->pev, pev->origin );
	m_glass->pev->owner = ENT( pev );
	m_glass->pev->angles = pev->angles;

	InitBoneControllers();
	SetBoneController(1, 360);

	CreateBeam();
	if (m_iJuice > 0)
	{
		m_iState = Still;
		SetThink(&CRechargeDecay::SearchForPlayer);
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		m_iState = Inactive;
	}
}

LINK_ENTITY_TO_CLASS(item_recharge, CRechargeDecay)

void CRechargeDecay::Precache(void)
{
	PRECACHE_MODEL("models/hev.mdl");
	PRECACHE_MODEL("models/hev_glass.mdl");
	PRECACHE_SOUND( "items/suitcharge1.wav" );
	PRECACHE_SOUND( "items/suitchargeno1.wav" );
	PRECACHE_SOUND( "items/suitchargeok1.wav" );
	PRECACHE_MODEL( "sprites/lgtning.spr" );
}

void CRechargeDecay::SearchForPlayer()
{
	CBaseEntity* pEntity = 0;
	float delay = 0.05;
	UTIL_MakeVectors( pev->angles );
	while((pEntity = UTIL_FindEntityInSphere(pEntity, Center(), 64)) != 0) { // this must be in sync with PLAYER_SEARCH_RADIUS from player.cpp
		if (pEntity->IsPlayer()) {
			if (DotProduct(pEntity->pev->origin - pev->origin, gpGlobals->v_forward) < 0) {
				continue;
			}
			TurnChargeToPlayer(pEntity->pev->origin);
			switch (m_iState) {
			case RetractShot:
				SetChargeState(Idle);
				break;
			case RetractArm:
				SetChargeState(Deploy);
				break;
			case Still:
				SetChargeState(Deploy);
				delay = 0.1;
				break;
			case Deploy:
				SetChargeState(Idle);
				break;
			case Idle:
				break;
			default:
				break;
			}
		}
		break;
	}
	if (!pEntity || !pEntity->IsPlayer()) {
		switch (m_iState) {
		case Deploy:
		case Idle:
		case RetractShot:
			SetChargeState(RetractArm);
			delay = 0.2;
			break;
		case RetractArm:
			SetChargeState(Still);
			break;
		case Still:
			break;
		default:
			break;
		}
	}
	pev->nextthink = gpGlobals->time + delay;
}

void CRechargeDecay::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	// Make sure that we have a caller
	if( !pActivator )
		return;
	// if it's not a player, ignore
	if( !pActivator->IsPlayer() )
		return;

	if (m_iState != Idle && m_iState != GiveShot && m_iState != Healing && m_iState != Inactive)
		return;

	// if there is no juice left, turn it off
	if( (m_iState == Healing || m_iState == GiveShot) && m_iJuice <= 0 )
	{
		pev->skin = 1;
		SetThink(&CRechargeDecay::Off);
		pev->nextthink = gpGlobals->time;
	}

	// if the player doesn't have the suit, or there is no juice left, make the deny noise
	if( ( m_iJuice <= 0 ) || ( !( pActivator->pev->weapons & ( 1 << WEAPON_SUIT ) ) ) )
	{
		if( m_flSoundTime <= gpGlobals->time )
		{
			m_flSoundTime = gpGlobals->time + 0.62;
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/suitchargeno1.wav", 1.0, ATTN_NORM );
		}
		return;
	}

	SetThink(&CRechargeDecay::Off);
	pev->nextthink = gpGlobals->time + 0.25;

	// Time to recharge yet?
	if( m_flNextCharge >= gpGlobals->time )
		return;

	TurnChargeToPlayer(pActivator->pev->origin);
	switch (m_iState) {
	case Idle:
		m_flSoundTime = 0.56 + gpGlobals->time;
		SetChargeState(GiveShot);
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/suitchargeok1.wav", 1.0, ATTN_NORM );
		break;
	case GiveShot:
		SetChargeState(Healing);
		break;
	case Healing:
		if (!m_playingChargeSound && m_flSoundTime <= gpGlobals->time)
		{
			m_playingChargeSound = TRUE;
			EMIT_SOUND( ENT( pev ), CHAN_STATIC, "items/suitcharge1.wav", 1.0, ATTN_NORM );
		}
		// We need to keep playing animation even though it's 1 frame only for controllers smoothing
		SetChargeState(Healing);
		break;
	default:
		ALERT(at_console, "Unexpected recharger state on use: %d\n", m_iState);
		break;
	}

	// charge the player
	if( pActivator->pev->armorvalue < 100 )
	{
		m_iJuice--;
		pActivator->pev->armorvalue += 1;
		const float boneControllerValue = (m_iJuice / gSkillData.suitchargerCapacity) * 360;
		SetBoneController(1, 360 - boneControllerValue);
		SetBoneController(2,  boneControllerValue);

		if( pActivator->pev->armorvalue > 100 )
			pActivator->pev->armorvalue = 100;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + 0.1;
}

void CRechargeDecay::Recharge( void )
{
//	/EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/suitcharge1.wav", 1.0, ATTN_NORM );
	m_iJuice = gSkillData.healthchargerCapacity;
	SetBoneController(1, 360);
	SetBoneController(2, 0);
	if (m_beam)
		m_beam->SetBrightness( 225 );
	pev->skin = 0;
	SetChargeState(Still);
	SetThink( &CRechargeDecay::SearchForPlayer );
	pev->nextthink = gpGlobals->time;
}

void CRechargeDecay::Off( void )
{
	switch (m_iState) {
	case GiveShot:
	case Healing:
		if (m_playingChargeSound) {
			STOP_SOUND( ENT( pev ), CHAN_STATIC, "items/suitcharge1.wav" );
			m_playingChargeSound = FALSE;
		}
		SetChargeState(RetractShot);
		pev->nextthink = gpGlobals->time + 0.1;
		break;
	case RetractShot:
		if (m_iJuice > 0) {
			SetChargeState(Idle);
			SetThink( &CRechargeDecay::SearchForPlayer );
			pev->nextthink = gpGlobals->time;
		} else {
			SetChargeState(RetractArm);
			pev->nextthink = gpGlobals->time + 0.2;
		}
		break;
	case RetractArm:
	{
		if( ( m_iJuice <= 0 ) )
		{
			if (m_beam)
				m_beam->SetBrightness(0);
			SetChargeState(Inactive);
			const float rechargeTime = g_pGameRules->FlHEVChargerRechargeTime();
			if (rechargeTime > 0 ) {
				pev->nextthink = gpGlobals->time + rechargeTime;
				SetThink( &CRechargeDecay::Recharge );
			}
		}
		break;
	}
	default:
		break;
	}
}

void CRechargeDecay::SetMySequence(const char *sequence)
{
	pev->sequence = LookupSequence( sequence );
	if (pev->sequence == -1) {
		ALERT(at_error, "unknown sequence: %s\n", sequence);
		pev->sequence = 0;
	}
	pev->frame = 0;
	ResetSequenceInfo( );
}

void CRechargeDecay::SetChargeState(int state)
{
	m_iState = state;
	if (state == RetractArm)
		SetChargeController(0);
	switch (state) {
	case Still:
		SetMySequence("rest");
		break;
	case Deploy:
		SetMySequence("deploy");
		break;
	case Idle:
		SetMySequence("prep_charge");
		break;
	case GiveShot:
		SetMySequence("give_charge");
		break;
	case Healing:
		SetMySequence("charge_idle");
		break;
	case RetractShot:
		SetMySequence("retract_charge");
		break;
	case RetractArm:
		SetMySequence("retract_arm");
		break;
	case Inactive:
		SetMySequence("rest");
	default:
		break;
	}
}

void CRechargeDecay::TurnChargeToPlayer(const Vector player)
{
	float yaw = UTIL_VecToYaw( player - pev->origin ) - pev->angles.y;

	if( yaw > 180 )
		yaw -= 360;
	if( yaw < -180 )
		yaw += 360;

	SetChargeController( yaw );
}

void CRechargeDecay::SetChargeController(float yaw)
{
	SetBoneController(3, yaw);
}

void CRechargeDecay::CreateBeam()
{
	CBeam* beam = CBeam::BeamCreate( "sprites/lgtning.spr", 5 );
	if( !beam )
		return;
	beam->EntsInit(entindex(), entindex());
	beam->SetStartAttachment(3);
	beam->SetEndAttachment(4);
	beam->SetColor( 0, 225, 0 );
	beam->SetBrightness( 225 );
	beam->SetNoise( 10 );
	beam->RelinkBeam();

	m_beam = beam;
}
