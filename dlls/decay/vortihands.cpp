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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

#define	CROWBAR_BODYHIT_VOLUME 128
#define	CROWBAR_WALLHIT_VOLUME 512
#define GAUSS_PRIMARY_CHARGE_VOLUME	256// how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME	450// how loud gauss is when discharged

extern int gmsgAlienState;
extern int g_irunninggausspred;
LINK_ENTITY_TO_CLASS( weapon_vorti, CVortiHands )

enum vortihands_e
{
	VORTIHANDS_IDLE1 = 0,
	VORTIHANDS_IDLE2,
	VORTIHANDS_ATTACK1,
	VORTIHANDS_ATTACK1MISS,
	VORTIHANDS_ATTACK2,
	VORTIHANDS_ATTACK2HIT,
	VORTIHANDS_ATTACK3,	
	VORTIHANDS_ATTACK3HIT,
	VORTIHANDS_CHARGE,
	VORTIHANDS_CHARGE_LOOP,
	VORTIHANDS_ZAP,
	VORTIHANDS_RETURN
};

float CVortiHands::GetFullChargeTime( void )
{
	return 3.0f;
}

void CVortiHands::Spawn()
{
	Precache();
	m_iId = WEAPON_VORTIHANDS;
	SET_MODEL( ENT( pev ), "models/w_slave.mdl" );
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

void CVortiHands::Precache( void )
{
	PRECACHE_MODEL( "models/v_slave.mdl" );
	PRECACHE_MODEL( "models/w_slave.mdl" );
	PRECACHE_MODEL( "models/p_slave.mdl" );
	PRECACHE_MODEL( "sprites/lgtning.spr" );
	PRECACHE_SOUND( "debris/zap1.wav" );
	PRECACHE_SOUND( "debris/zap4.wav" );
	PRECACHE_SOUND( "zombie/claw_strike1.wav" );
	PRECACHE_SOUND( "zombie/claw_strike2.wav" );
	PRECACHE_SOUND( "zombie/claw_strike3.wav" );
	PRECACHE_SOUND( "zombie/claw_miss1.wav" );
	PRECACHE_SOUND( "zombie/claw_miss2.wav" );
	PRECACHE_SOUND( "aslave/slv_word5.wav" );
	PRECACHE_SOUND( "aslave/slv_word7.wav" );
	PRECACHE_SOUND( "aslave/slv_word3.wav" );
	PRECACHE_SOUND( "aslave/slv_word4.wav" );

	m_usVorti = PRECACHE_EVENT( 1, "events/vorti.sc" );
	m_usVortiFire = PRECACHE_EVENT( 1, "events/vortifire.sc" );
	m_usVortiSpin = PRECACHE_EVENT( 1, "events/vortispin.sc" );
}

int CVortiHands::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iId = WEAPON_VORTIHANDS;
	p->iWeight = VORTIHANDS_WEIGHT;
	return 1;
}

int CVortiHands::AddToPlayer( CBasePlayer *pPlayer )
{
	if( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

void CVortiHands::CrosshairState( int state )
{
#ifndef CLIENT_DLL
	MESSAGE_BEGIN( MSG_ONE, gmsgAlienState, NULL, m_pPlayer->pev );
		WRITE_BYTE( state );
	MESSAGE_END();
#endif
}

BOOL CVortiHands::Deploy()
{
	return DefaultDeploy( "models/v_slave.mdl", "models/p_slave.mdl", VORTIHANDS_IDLE1, "vortihands" );
}

void CVortiHands::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( VORTIHANDS_IDLE1 );
}

extern void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity );
/*
void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int		i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult	tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ( ( vecHullEnd - vecSrc ) * 2 );
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for( i = 0; i < 2; i++ )
	{
		for( j = 0; j < 2; j++ )
		{
			for( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = ( tmpTrace.vecEndPos - vecSrc ).Length();
					if( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}
*/

void CVortiHands::PrimaryAttack()
{
	m_fInAttack = 0;
	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	CrosshairState( m_fInAttack );
	if( !Swing( 1 ) )
	{
#ifndef CLIENT_DLL
		SetThink( &CVortiHands::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
#endif
	}
}

void CVortiHands::Smack()
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

void CVortiHands::SwingAgain( void )
{
	Swing( 0 );
}

int CVortiHands::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif
	if( fFirst )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usVorti, 
		0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
		0, 0, 0 );
	}

	if( tr.flFraction >= 1.0 )
	{
		if( fFirst )
		{
			// miss
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		switch( ( ( m_iSwing++ ) % 2 ) + 1 )
		{
		case 0:
			SendWeaponAnim( VORTIHANDS_ATTACK1 );
			break;
		case 1:
			SendWeaponAnim( VORTIHANDS_ATTACK2HIT );
			break;
		case 2:
			SendWeaponAnim( VORTIHANDS_ATTACK3HIT );
			break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
		// hit
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		// play thwack, smack, or dong sound
                float flVol = 1.0;
                int fHitWorld = TRUE;

		if( pEntity )
		{
			ClearMultiDamage();
			// If building with the clientside weapon prediction system,
			// UTIL_WeaponTimeBase() is always 0 and m_flNextPrimaryAttack is >= -1.0f, thus making
			// m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() always evaluate to false.
#ifdef CLIENT_WEAPONS
			if( ( m_flNextPrimaryAttack + 1 == UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
#else
			if( ( m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
#endif
			{
				// first swing does full damage
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.plrDmgCrowbar, gpGlobals->v_forward, &tr, DMG_CLUB ); 
			}
			else
			{
				// subsequent swings do half
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.plrDmgCrowbar / 2, gpGlobals->v_forward, &tr, DMG_CLUB ); 
			}
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

			if( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG( 0, 2 ) )
				{
				case 0:
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "zombie/claw_strike1.wav", 1, ATTN_NORM );
					break;
				case 1:
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "zombie/claw_strike2.wav", 1, ATTN_NORM );
					break;
				case 2:
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "zombie/claw_strike3.wav", 1, ATTN_NORM );
					break;
				}
				m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
				if( !pEntity->IsAlive() )
					return TRUE;
				else
					flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if( fHitWorld )
		{
			float fvolbar = TEXTURETYPE_PlaySound( &tr, vecSrc, vecSrc + ( vecEnd - vecSrc ) * 2, BULLET_PLAYER_CROWBAR );

			if( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play crowbar strike
			switch( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "zombie/claw_miss1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) ); 
				break;
			case 1:
				EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "zombie/claw_miss2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = (int)( flVol * CROWBAR_WALLHIT_VOLUME );

		SetThink( &CVortiHands::Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.2;
#endif
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.25 );
	}
	return fDidHit;
}

void CVortiHands::SecondaryAttack()
{
	// don't fire underwater
	if( m_pPlayer->pev->waterlevel == 3 )
	{
		if( m_fInAttack != 0 )
		{
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG( 0, 0x3f ) );
			SendWeaponAnim( VORTIHANDS_IDLE1 );
			m_fInAttack = 0;
		}
		else
		{
			PlayEmptySound();
		}

		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
		return;
	}

	if( m_fInAttack == 0 )
	{
		// spin up
		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;

		m_fInAttack = 1;
		CrosshairState( m_fInAttack );
		SendWeaponAnim( VORTIHANDS_CHARGE );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
		m_pPlayer->m_flStartCharge = gpGlobals->time;
		m_pPlayer->m_flAmmoStartCharge = UTIL_WeaponTimeBase() + GetFullChargeTime();

		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usVortiSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 110, 0, 0, 0 );

		m_iSoundState = SND_CHANGE_PITCH;
	}
	else if( m_fInAttack == 1 )
	{
		if( m_flTimeWeaponIdle < UTIL_WeaponTimeBase() )
		{
			SendWeaponAnim( VORTIHANDS_CHARGE_LOOP );
			m_fInAttack = 2;
			CrosshairState( m_fInAttack );
		}
	}
	else
	{
		int pitch = (int)( ( gpGlobals->time - m_pPlayer->m_flStartCharge ) * ( 150 / GetFullChargeTime() ) + 100 );
		if( pitch > 250 ) 
			 pitch = 250;
		
		// ALERT( at_console, "%d %d %d\n", m_fInAttack, m_iSoundState, pitch );

		if( m_iSoundState == 0 )
			ALERT( at_console, "sound state %d\n", m_iSoundState );
		CrosshairState( 3 );
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usVortiSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, pitch, 0, ( m_iSoundState == SND_CHANGE_PITCH ) ? 1 : 0, 0 );

		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions

		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;

		// m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		if( m_pPlayer->m_flStartCharge < gpGlobals->time - 10 )
		{
			// Player charged up too long. Zap him.
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG( 0, 0x3f ) );
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/electro6.wav", 1.0, ATTN_NORM, 0, 75 + RANDOM_LONG( 0, 0x3f ) );

			m_fInAttack = 0;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
			CrosshairState( m_fInAttack );
			SendWeaponAnim( VORTIHANDS_IDLE1 );

			// Player may have been killed and this weapon dropped, don't execute any more code after this!
			return;
		}
	}
}

//=========================================================
// StartFire- since all of this code has to run and then 
// call Fire(), it was easier at this point to rip it out 
// of weaponidle() and make its own function then to try to
// merge this into Fire(), which has some identical variable names 
//=========================================================
void CVortiHands::StartFire( void )
{
	float flDamage;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition(); // + gpGlobals->v_up * -8 + gpGlobals->v_right * 8;

	if( gpGlobals->time - m_pPlayer->m_flStartCharge < GetFullChargeTime() )
	{
		flDamage = 60;
	}
	else
	{
		flDamage = 60 * ( ( gpGlobals->time - m_pPlayer->m_flStartCharge ) / GetFullChargeTime() );
	}


	if( m_fInAttack != 3 )
	{
		//ALERT( at_console, "Time:%f Damage:%f\n", gpGlobals->time - m_pPlayer->m_flStartCharge, flDamage );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}

	// time until aftershock 'static discharge' sound
	m_pPlayer->m_flPlayAftershock = gpGlobals->time + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.3, 0.8 );

	Fire( vecSrc, vecAiming, flDamage );
}

void CVortiHands::Fire( Vector vecOrigSrc, Vector vecDir, float flDamage )
{
	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	TraceResult tr, beam_tr;
#ifndef CLIENT_DLL
	Vector vecSrc = vecOrigSrc;
	Vector vecDest = vecSrc + vecDir * 8192;
	edict_t	*pentIgnore;
	float flMaxFrac = 1.0;
	int nTotal = 0;
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int nMaxHits = 10;

	pentIgnore = ENT( m_pPlayer->pev );
#else
		 g_irunninggausspred = true;
#endif	
	// The main firing event is sent unreliably so it won't be delayed.
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usVortiFire, 0.0, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, flDamage, 0.0, 0, 0, 0, 0 );

	// This reliable event is used to stop the spinning sound
	// It's delayed by a fraction of second to make sure it is delayed by 1 frame on the client
	// It's sent reliably anyway, which could lead to other delays

	PLAYBACK_EVENT_FULL( FEV_NOTHOST | FEV_RELIABLE, m_pPlayer->edict(), m_usVortiFire, 0.01, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0.0, 0.0, 0, 0, 0, 1 );

	/*ALERT( at_console, "%f %f %f\n%f %f %f\n", 
		vecSrc.x, vecSrc.y, vecSrc.z, 
		vecDest.x, vecDest.y, vecDest.z );*/

	//ALERT( at_console, "%f %f\n", tr.flFraction, flMaxFrac );

#ifndef CLIENT_DLL
	while( flDamage > 10 && nMaxHits > 0 )
	{
		nMaxHits--;

		// ALERT( at_console, "." );
		UTIL_TraceLine( vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr );

		if( tr.fAllSolid )
			break;

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if( pEntity == NULL )
			break;

		if( fFirstBeam )
		{
			m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
			fFirstBeam = 0;

			nTotal += 26;
		}

		if( pEntity->pev->takedamage )
		{
			ClearMultiDamage();
			pEntity->TraceAttack( m_pPlayer->pev, flDamage, vecDir, &tr, DMG_BULLET );
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
		}

		vecSrc = tr.vecEndPos + vecDir;
		pentIgnore = ENT( pEntity->pev );
	}
#endif
	// ALERT( at_console, "%d bytes\n", nTotal );
}

void CVortiHands::WeaponIdle( void )
{
	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if( m_fInAttack != 0 )
	{
		m_fInAttack = 0;
		CrosshairState( m_fInAttack );
		ALERT( at_console, "released right button!\n" );

		StartFire();
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
	}
}
