/***
Created by Solexid
*
****/



#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "soundent.h"
#include "weapons.h"
#include "nodes.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"

#define	GRAV_BEAM_SPRITE_PRIMARY_VOLUME		30
#define GRAV_BEAM_SPRITE		"sprites/xbeam3.spr"
#define GRAV_FLARE_SPRITE		"sprites/hotglow.spr"
#define GRAV_SOUND_OFF			"buttons/latchunlocked1.wav"
#define GRAV_SOUND_RUN			"weapons/mine_activate.wav"
#define GRAV_SOUND_FAILRUN		"houndeye/he_die3.wav"
#define GRAV_SOUND_STARTUP		"weapons/gauss2.wav"
#define WEAPON_GRAVGUN 17
#define EGON_SWITCH_NARROW_TIME			0.75			// Time it takes to switch fire modes

enum gauss_e {
	GAUSS_IDLE = 0,
	GAUSS_IDLE2,
	GAUSS_FIDGET,
	GAUSS_SPINUP,
	GAUSS_SPIN,
	GAUSS_FIRE,
	GAUSS_FIRE2,
	GAUSS_HOLSTER,
	GAUSS_DRAW
};

class CGrav : public CBasePlayerWeapon
{
public:


	void Spawn(void);
	void Precache(void);
	int iItemSlot(void) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer(CBasePlayer *pPlayer);
	int m_iStage = 0;
	BOOL Deploy(void);
	void Holster(int skiplocal = 0);
	int m_iGrabFailures = 0;
	EHANDLE m_hAimentEntity;
	void UpdateEffect(const Vector &startPoint, const Vector &endPoint, float timeBlend);
	CBaseEntity *	TraceForward(CBaseEntity *pMe, float radius);
	void CreateEffect(void);
	void DestroyEffect(void);
	void EndAttack(void);
	void Attack(void);
	void Attack2(void);
	void PrimaryAttack(void);
	void SecondaryAttack(void);
	void WeaponIdle(void);
	void Pull(CBaseEntity* ent, float force);
	void GravAnim(int iAnim, int skiplocal, int body);
	CBaseEntity *GetCrossEnt( Vector gunpos, Vector aim );
	float m_flNextGravgunAttack = gpGlobals->time;
	float m_flAmmoUseTime;// since we use < 1 point of ammo per update, we subtract ammo on a timer.


	void GrabThink(void);
	void Fire(const Vector &vecOrigSrc, const Vector &vecDir);

	BOOL HasAmmo(void);


	enum GRAV_FIREMODE { FIRE_NARROW, FIRE_WIDE };

	CBeam				*m_pBeam;
	CBeam				*m_pNoise;
	CSprite				*m_pSprite;

	virtual BOOL UseDecrement(void)
	{
		return false;
	}

private:
	float				m_shootTime;
	GRAV_FIREMODE		m_fireMode;
	float				m_shakeTime;
	BOOL				m_deployed;
	float				m_fPushSpeed;
};

LINK_ENTITY_TO_CLASS(weapon_gravgun, CGrav);

void CGrav::Spawn()
{
	pev->classname = MAKE_STRING("weapon_gravgun"); // hack to allow for old names
	Precache();
	m_iId = WEAPON_GRAVGUN;
	SET_MODEL(ENT(pev), "models/w_gravcannon.mdl");
	m_iClip = -1;
	m_iDefaultAmmo = -1;

	FallInit();// get ready to fall down.
}


void CGrav::Precache(void)
{
	PRECACHE_MODEL("models/w_gravcannon.mdl");
	PRECACHE_MODEL("models/v_gravcannon.mdl");
	PRECACHE_MODEL("models/p_gravcannon.mdl");

	PRECACHE_MODEL("models/w_9mmclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");
	
	PRECACHE_SOUND(GRAV_SOUND_OFF);
	PRECACHE_SOUND(GRAV_SOUND_RUN); 
	PRECACHE_SOUND(GRAV_SOUND_FAILRUN);
	PRECACHE_SOUND(GRAV_SOUND_STARTUP);

	PRECACHE_MODEL(GRAV_BEAM_SPRITE);
	PRECACHE_MODEL("sprites/hotglow.spr");

	PRECACHE_SOUND("weapons/357_cock1.wav");

}


BOOL CGrav::Deploy(void)
{
	m_deployed = FALSE;
	m_fireState = FIRE_OFF;
	return DefaultDeploy("models/v_gravcannon.mdl", "models/p_gravcannon.mdl", GAUSS_DRAW, "gauss");
}

int CGrav::AddToPlayer(CBasePlayer *pPlayer)
{
	if (CBasePlayerWeapon::AddToPlayer(pPlayer))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
		WRITE_BYTE(m_iId);
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}



void CGrav::Holster(int skiplocal /* = 0 */)
{
	
	SetThink(NULL);
	if (m_hAimentEntity) { m_hAimentEntity->pev->velocity = Vector(0, 0, 0); }
	m_hAimentEntity = NULL;
	EndAttack();
	m_iStage = 0;
	m_flNextGravgunAttack = gpGlobals->time + 0.5;
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	GravAnim(GAUSS_HOLSTER,0,0);
	SetThink(NULL);
}

int CGrav::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_GRAVGUN;
	p->iFlags = 0;
	p->iWeight = 20;

	return 1;
}

//#define USEGUN 1


BOOL CGrav::HasAmmo(void)
{
	
	return TRUE;
}



void CGrav::Attack(void)
{
	pev->nextthink = gpGlobals->time + 1.1;
	m_flNextGravgunAttack - gpGlobals->time + 0.5;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition();

	switch (m_fireState)
	{
	case FIRE_OFF:
	{
		m_fireMode = FIRE_WIDE;


		GravAnim(GAUSS_FIRE2, 1, 0);
		m_pPlayer->m_iWeaponVolume = 20;
		m_flTimeWeaponIdle = gpGlobals->time + 0.04;
		pev->fuser1 = gpGlobals->time + 0.1;
		
		m_iStage = 0;
		m_fireState = FIRE_CHARGE;
	}
	break;

	case FIRE_CHARGE:
	{
		Fire(vecSrc, vecAiming);
		m_pPlayer->m_iWeaponVolume = 20;

		if (pev->fuser1 <= gpGlobals->time)
		{
			GravAnim(GAUSS_SPIN, 1, 0);
			pev->fuser1 = 1000;
			SetThink(NULL);
		}
		

		//CBaseEntity* crosent = TraceForward(m_pPlayer, 1000);
		CBaseEntity* crossent = m_hAimentEntity;
		m_hAimentEntity = NULL;
		if( !crossent)
			crossent = GetCrossEnt(vecSrc, gpGlobals->v_forward);
		//int oc = 0;
		if (crossent) {
			m_flNextGravgunAttack = gpGlobals->time + 0.8;
			//oc = crosent->ObjectCaps();

			//int propc = (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE;
			if( !(m_fPushSpeed = crossent->TouchGravGun(m_pPlayer,3) ) )
			{
				crossent = TraceForward(m_pPlayer, 1000);
				if( !crossent || !(m_fPushSpeed = crossent->TouchGravGun(m_pPlayer,3)) )
				{
					EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, GRAV_SOUND_FAILRUN, 0.6, ATTN_NORM, 0, 70 + RANDOM_LONG(0, 34));
					crossent = NULL;
				}
			}

			if(crossent) {

				
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, GRAV_SOUND_STARTUP, 1, ATTN_NORM, 0, 70 + RANDOM_LONG(0, 34));
					
					//if (crosent->pev->flags& FL_ONGROUND) { pev->velocity = pev->velocity * 0.95; };
		
					crossent->TouchGravGun(m_pPlayer,3);
					Vector pusher = vecAiming;
					pusher.x = pusher.x * m_fPushSpeed;
					pusher.y = pusher.y * m_fPushSpeed;
					pusher.z = pusher.z * m_fPushSpeed * 0.7;
					crossent->pev->velocity = pusher+m_pPlayer->pev->velocity;
					crossent->pev->avelocity.y = pev->avelocity.y*3.5 + RANDOM_FLOAT(100, -100);
					crossent->pev->avelocity.x = pev->avelocity.x*3.5 + RANDOM_FLOAT(100, -100);
					crossent->pev->avelocity.z = pev->avelocity.z + 3;
				

			}
		}
		if (gpGlobals->time >= m_flNextGravgunAttack)
		{
			m_flNextGravgunAttack = gpGlobals->time + 0.7;
			EndAttack();
		}
		if (m_fireMode = FIRE_NARROW) { EndAttack(); }


	}
	
		m_flNextGravgunAttack = gpGlobals->time + 0.5;
	
	break;
	}

}
void CGrav::GravAnim(int iAnim, int skiplocal, int body)
{

	m_pPlayer->pev->weaponanim = iAnim;



	MESSAGE_BEGIN(MSG_ONE, SVC_WEAPONANIM, NULL, m_pPlayer->pev);
	WRITE_BYTE(iAnim); // sequence number
	WRITE_BYTE(pev->body); // weaponmodel bodygroup.
	MESSAGE_END();
}

void CGrav::Attack2(void)
{
	//if (temp) { temp = NULL; }
	//if(temp) return;
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecAiming = gpGlobals->v_forward;
	//Vector vecAiming = UTIL_GetAimVector(m_pPlayer->edict(), 1000);
	Vector vecSrc = m_pPlayer->GetGunPosition();

	switch (m_fireState)
	{
	case FIRE_OFF:
	{
		GravAnim(GAUSS_FIRE, 1, 0);
					 m_pPlayer->m_iWeaponVolume = 20;
				
					 m_fireState = FIRE_CHARGE;
				
	}
		break;

	case FIRE_CHARGE:
	{
						Fire(vecSrc, vecAiming);
						m_pPlayer->m_iWeaponVolume = 100;

						if (pev->fuser1 <= gpGlobals->time)
						{
								
							pev->fuser1 = 1000;
						}
						//CBaseEntity* crossent = TraceForward(m_pPlayer,500);
						CBaseEntity* crossent = GetCrossEnt(vecSrc, gpGlobals->v_forward);
						if( !crossent || !(m_fPushSpeed = crossent->TouchGravGun(m_pPlayer,3)) )
						{
							crossent = TraceForward(m_pPlayer, 1000);
							if( !crossent || !(m_fPushSpeed = crossent->TouchGravGun(m_pPlayer,3)) )
							{
								EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, GRAV_SOUND_FAILRUN, 0.6, ATTN_NORM, 0, 70 + RANDOM_LONG(0, 34));
								crossent = NULL;
							}
						}
						if ( crossent ){
							EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, GRAV_SOUND_RUN, 0.6, ATTN_NORM, 0, 70 + RANDOM_LONG(0, 34));
							if(crossent->TouchGravGun(m_pPlayer, 0))
							{
								m_hAimentEntity = crossent;
								Pull(crossent,5);
								GravAnim(GAUSS_SPIN, 0, 0);
							}
					
						}
					
	}
		break;
	}

}
CBaseEntity *CGrav::GetCrossEnt( Vector gunpos, Vector aim )
{
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	edict_t		*pClosest = NULL;
	Vector		vecLOS;
	float flMaxDot = 0.4;
	float flDot;

	if ( !pEdict )
		return NULL;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		if( pEdict == m_pPlayer->edict() )
			continue;
		vecLOS = pEdict->v.absmin + ( pEdict->v.size * 0.5 ) - gunpos;
		vecLOS = UTIL_ClampVectorToBox(vecLOS, pEdict->v.size * 0.5);

		flDot = DotProduct(vecLOS, aim);
		if (flDot > flMaxDot)
		{
			pClosest = pEdict;
			flMaxDot = flDot;

		}

	}
	return GET_PRIVATE(pClosest);

}

CBaseEntity*  CGrav::TraceForward(CBaseEntity *pMe,float radius)
{

#ifdef USEGUN
	


	CBaseEntity *pObject = NULL;
	CBaseEntity *pClosest = NULL;
	Vector		vecLOS;
	float flMaxDot = 0.4;
	float flDot;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);// so we know which way we are facing

	while ((pObject = UTIL_FindEntityInSphere(pObject, m_pPlayer->pev->origin, radius)) != NULL)
	{//pObject->ObjectCaps() & (FCAP_ACROSS_TRANSITION | FCAP_CONTINUOUS_USE )&&
		if (pObject!=m_pPlayer) {
			vecLOS = (VecBModelOrigin(pObject->pev) - (m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs));
			vecLOS = UTIL_ClampVectorToBox(vecLOS, pObject->pev->size * 0.5);

			flDot = DotProduct(vecLOS, gpGlobals->v_forward);
			if (flDot > flMaxDot&&pClosest != m_pPlayer)
			{
				pClosest = pObject;
				flMaxDot = flDot;

			}
			ALERT(at_console, "%s : %f\n", STRING(pObject->pev->classname), flDot);
		}
	}

		pObject = pClosest;
	
	
	return pObject;





#else
	TraceResult tr;

	UTIL_MakeVectors(pMe->pev->v_angle);
	UTIL_TraceLine(pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * radius, missile, pMe->edict(), &tr);
	if (tr.flFraction != 1.0 && !FNullEnt(tr.pHit))
	{
		CBaseEntity *pHit = CBaseEntity::Instance(tr.pHit);
		return pHit;
	}
#endif
	return NULL;
}
//Failure traces counter for GrabThink

//Used for prop grab and 
void CGrav::GrabThink()
{
//CBaseEntity *ent = FindEntityForward4(m_pPlayer, 130);
	

	if (( m_iGrabFailures < 50 )&& m_hAimentEntity && !m_hAimentEntity->pev->deadflag)
	{
			if( ( m_hAimentEntity->pev->origin - m_pPlayer->pev->origin).Length() > 150 )
				m_iGrabFailures++;
			else
				m_iGrabFailures = 0;

			UpdateEffect(pev->origin, m_hAimentEntity->pev->origin, 1);

			Pull(m_hAimentEntity, 100);

			pev->nextthink = gpGlobals->time + 0.001;
	}
	else{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, GRAV_SOUND_OFF, 1, ATTN_NORM, 0, 70 + RANDOM_LONG(0, 34));
		m_iGrabFailures = 0;
		SetThink(NULL);
		if(m_hAimentEntity)
		{
			m_hAimentEntity->pev->velocity = Vector(0,0,0);
			m_hAimentEntity = NULL;
		}
		EndAttack();
		m_iStage = 0;
	}



}
void CGrav::Pull(CBaseEntity* ent,float force)
{
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	ent->pev->angles.x = UTIL_AngleMod(ent->pev->angles.x);
	ent->pev->angles.y = UTIL_AngleMod(ent->pev->angles.y);
	ent->pev->angles.z = UTIL_AngleMod(ent->pev->angles.z);

	Vector target = m_pPlayer->pev->origin + gpGlobals->v_forward * 75;
	target.z += 32;
	if ((target - VecBModelOrigin(ent->pev)).Length() > 60){
		target = m_pPlayer->pev->origin + gpGlobals->v_forward * 110 ;

		target.z += 60;
		

		ALERT(at_console, "%s 1 %d : %f\n", STRING(ent->pev->classname), m_iStage, ((target - VecBModelOrigin(ent->pev)).Length()));

	
		if( !m_iStage )
		{
			ent->pev->velocity = (target - VecBModelOrigin( ent->pev )).Normalize()*300;
			pev->velocity.z += 10;
			if( (target - VecBModelOrigin( ent->pev )).Length() < 150 )
			{
				m_iStage = 1;
				SetThink( &CGrav::GrabThink );
				pev->nextthink = gpGlobals->time + 0.001;
				ent->TouchGravGun(m_pPlayer, 1);
			}
		}
		else
		{
			ent->pev->velocity = (target - VecBModelOrigin(ent->pev)).Normalize()*550;
			pev->velocity.z += 15;
		}
		ent->pev->velocity = ent->pev->velocity + m_pPlayer->pev->velocity;
			/////
#ifdef BEAMS
		CBeam* m_pBeam1 = CBeam::BeamCreate(GRAV_BEAM_SPRITE, 40);
		m_pBeam1->SetFlags(BEAM_FSHADEOUT);
		m_pBeam1->pev->spawnflags |= SF_BEAM_TEMPORARY;	// Flag these to be destroyed on save/restore or level transition
		m_pBeam1->pev->flags |= FL_SKIPLOCALHOST;
		m_pBeam1->pev->owner = m_pPlayer->edict();
		m_pBeam1->SetStartPos(target);
		m_pBeam1->SetEndPos(VecBModelOrigin(ent->pev));
		m_pBeam1->SetWidth(40 - (1 * 20));
		m_pBeam1->SetBrightness(130);
#endif // BEAMS

		
		/////
		ALERT(at_console, "%s 2: %f\n", STRING(ent->pev->classname), m_iStage, ent->pev->velocity.Length());
	}
	else
	{	
		ent->pev->velocity = (target - VecBModelOrigin(ent->pev))* 40;
		if(ent->pev->velocity.Length()>900)
			ent->pev->velocity = (target - VecBModelOrigin(ent->pev)).Normalize() * 900;
		ent->pev->velocity = ent->pev->velocity + m_pPlayer->pev->velocity;
		m_iStage = 2;
		SetThink( &CGrav::GrabThink );
		pev->nextthink = gpGlobals->time + 0.001;
		ent->TouchGravGun(m_pPlayer, 2);
	}

	
	
	}





void CGrav::PrimaryAttack(void)
{
	if (m_flNextGravgunAttack < gpGlobals->time)
	{
	
	
	SetThink(NULL);
	Attack();

	}

}


void CGrav::SecondaryAttack(void)
{
	if (m_flNextGravgunAttack < gpGlobals->time)
	{
		if (m_iStage)
		{
			if( m_fireState != FIRE_OFF )
			{
				return;
			}
			EndAttack();
			SetThink(NULL);
			m_flNextGravgunAttack = gpGlobals->time + 0.6;
			//m_flTimeWeaponIdle = gpGlobals->time + 0.1;

			m_iStage = 0;
			if( m_hAimentEntity )
			{
				m_hAimentEntity->pev->velocity = Vector(0,0,0);
				m_hAimentEntity = NULL;
			}
		}
		else {
			m_fireMode = FIRE_NARROW;
			Attack2();
		}

	}
}
void CGrav::Fire(const Vector &vecOrigSrc, const Vector &vecDir)
{
	Vector vecDest = vecOrigSrc + vecDir * 2048;
	edict_t		*pentIgnore;
	TraceResult tr;

	pentIgnore = m_pPlayer->edict();
	Vector tmpSrc = vecOrigSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;
	UTIL_TraceLine(vecOrigSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

	if (tr.fAllSolid)
		return;

	UpdateEffect(tmpSrc, tr.vecEndPos, 1);
}


void CGrav::UpdateEffect(const Vector &startPoint, const Vector &endPoint, float timeBlend)
{
#ifndef CLIENT_DLL
	if (!m_pBeam)
	{
		CreateEffect();
	}

	m_pBeam->SetStartPos(endPoint);
	m_pBeam->SetBrightness(255 - (timeBlend * 180));
	m_pBeam->SetWidth(40 - (timeBlend * 20));

	if (m_fireMode == FIRE_WIDE)
		m_pBeam->SetColor(100 + (25 * timeBlend),  104 + 80 * fabs(sin(gpGlobals->time * 10)),10 );
	else
		m_pBeam->SetColor(90 + (25 * timeBlend), 100 + 80 * fabs(sin(gpGlobals->time * 10)), 10+(30 * timeBlend));
	Vector& lel=pev->origin;
	lel[0] = 30;
	lel[1] = 30;
	lel[2] = 30;

	//Vector& ar= m_pPlayer->pev-> origin+ m_pPlayer->pev->view_ofs;
	//Vector& al = pev->angles;

	
//	UTIL_SetOrigin(m_pSprite->pev, ar);
//	m_pSprite->pev->frame += 8 * gpGlobals->frametime;
	//if (m_pSprite->pev->frame > m_pSprite->Frames())
	//	m_pSprite->pev->frame = 0;

	m_pNoise->SetStartPos(endPoint);

#endif

}

void CGrav::CreateEffect(void)
{

#ifndef CLIENT_DLL
	DestroyEffect();
	
	m_pBeam = CBeam::BeamCreate(GRAV_BEAM_SPRITE, 40);
	m_pBeam->PointEntInit(pev->origin, m_pPlayer->entindex());
	m_pBeam->SetFlags(BEAM_FSINE);
	m_pBeam->SetEndAttachment(1);
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;	// Flag these to be destroyed on save/restore or level transition
	//m_pBeam->pev->flags |= FL_SKIPLOCALHOST;
	m_pBeam->pev->owner = m_pPlayer->edict();

	m_pNoise = CBeam::BeamCreate(GRAV_BEAM_SPRITE, 55);
	m_pNoise->PointEntInit(pev->origin, m_pPlayer->entindex());
	m_pNoise->SetScrollRate(3);
	m_pNoise->SetBrightness(100);
	m_pNoise->SetEndAttachment(1);
	m_pNoise->pev->spawnflags |= SF_BEAM_TEMPORARY;
	//m_pNoise->pev->flags |= FL_SKIPLOCALHOST;
	m_pNoise->pev->owner = m_pPlayer->edict();

	/*m_pSprite = CSprite::SpriteCreate(GRAV_FLARE_SPRITE, m_pPlayer->GetGunPosition(), TRUE);
	m_pSprite->pev->scale = 1.0;

	m_pSprite->SetTransparency(kRenderGlow, 255, 140, 0, 255, kRenderFxPulseFast);
	m_pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	m_pSprite->pev->flags |= FL_SKIPLOCALHOST;
	m_pSprite->pev->owner = m_pPlayer->edict();*/

	if (m_fireMode == FIRE_WIDE)
	{
		m_pBeam->SetScrollRate(300);
		m_pBeam->SetNoise(20);
		m_pNoise->SetColor(200, 120, 30);
		m_pNoise->SetNoise(8);
	}
	else
	{
		m_pBeam->SetScrollRate(200);
		m_pBeam->SetNoise(5);
		m_pNoise->SetColor(0, 255, 0);
		m_pNoise->SetNoise(2);
	}
#endif

}


void CGrav::DestroyEffect(void)
{

#ifndef CLIENT_DLL
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = NULL;
	}
	if (m_pNoise)
	{
		UTIL_Remove(m_pNoise);
		m_pNoise = NULL;
	}
	if (m_pSprite)
	{
		if (m_fireMode == FIRE_WIDE)
			m_pSprite->Expand(10, 500);
		else
			UTIL_Remove(m_pSprite);
		m_pSprite = NULL;
	}
#endif

}



void CGrav::WeaponIdle(void)
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_fireState != FIRE_OFF)
		EndAttack();

	GravAnim(GAUSS_IDLE, 0, 0);
	m_deployed = TRUE;
}



void CGrav::EndAttack(void)
{
	bool bMakeNoise = false;
   // if (m_AimentEntity&&m_AimentEntity->pev->velocity.Length() > 100&& (m_AimentEntity->pev->origin-m_pPlayer->pev->origin).Length()<100) { m_AimentEntity->pev->velocity = m_AimentEntity->pev->velocity / 10; }
	ALERT( at_console, "EndAttack()\n");
	if (m_fireState != FIRE_OFF) //Checking the button just in case!.
		bMakeNoise = true;
	m_flNextGravgunAttack = gpGlobals->time + 0.1;
	m_flTimeWeaponIdle = gpGlobals->time + 0.2;

	m_fireState = FIRE_OFF;

	DestroyEffect();
}




