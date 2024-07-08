//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: zombiea
// hl1 zombie with declarations to make it climb
//
// UNDONE: Make head take 100% damage, body take 30% damage.
// UNDONE: Don't flinch every time you get hit.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "game.h"
#include "AI_Default.h"
#include "AI_Schedule.h"
#include "AI_Hull.h"
#include "AI_Route.h"
#include "ai_basenpc.h"
#include "ai_motor.h"
#include "ai_memory.h"
#include "NPCEvent.h"
#include "hl1_npc_zombiea.h"
#include "gib.h"
//#include "AI_Interactions.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "ai_task.h"
#include "activitylist.h"
#include "movevars_shared.h"
#include "props.h"

ConVar	sk_zombiea_health("sk_zombiea_health", "50");
ConVar  sk_zombiea_dmg_one_slash("sk_zombiea_dmg_one_slash", "20");
ConVar  sk_zombiea_dmg_both_slash("sk_zombiea_dmg_both_slash", "40");



enum
{
	COND_FASTZOMBIE_CLIMB_TOUCH	= LAST_BASE_ZOMBIE_CONDITION,
};

enum
{
	SCHED_FASTZOMBIE_RANGE_ATTACK1 = LAST_SHARED_SCHEDULE + 100, // hack to get past the base zombie's schedules
	SCHED_FASTZOMBIE_UNSTICK_JUMP,
	SCHED_FASTZOMBIE_CLIMBING_UNSTICK_JUMP,
	SCHED_FASTZOMBIE_MELEE_ATTACK1,
	SCHED_FASTZOMBIE_TORSO_MELEE_ATTACK1,
};



unsigned char	m_iClimbCount;




LINK_ENTITY_TO_CLASS(monster_zombiea, CNPC_Zombiea);

void ClimbTouch(CBaseEntity *pOther);


//=========================================================
// Spawn
//=========================================================
void CNPC_Zombiea::Spawn()
{
	Precache();

	SetModel("models/infected/common_military_male01.mdl");

	//models/infected/common_military_male01.mdl

	SetRenderColor(255, 255, 255, 255);

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_STEP);
	m_bloodColor = BLOOD_COLOR_GREEN;
	m_iHealth = sk_zombiea_health.GetFloat();
	//pev->view_ofs		= VEC_VIEW;// position of the eyes relative to monster's origin.
	m_flFieldOfView = 0.5;
	m_NPCState = NPC_STATE_NONE;
	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_DOORS_GROUP);

	NPCInit();
}

#define ZOMBIEA_IGNORE_PLAYER 64


//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Zombiea::Precache()
{
	PrecacheModel("models/infected/common_military_male01.mdl");

	// PrecacheModel("models/infected/common_military_male01.mdl");

	PrecacheScriptSound("Zombie_Zomb.AttackHit");
	PrecacheScriptSound("Zombie_Zomb.AttackMiss");
	PrecacheScriptSound("Zombie_Zomb.Pain");
	PrecacheScriptSound("Zombie_Zomb.Idle");
	PrecacheScriptSound("Zombie_Zomb.Alert");
	PrecacheScriptSound("Zombie_Zomb.Attack");

	BaseClass::Precache();
}

Disposition_t CNPC_Zombiea::IRelationType(CBaseEntity *pTarget)
{
	if ((pTarget->IsPlayer()))
	{
		if ((GetSpawnFlags() & ZOMBIEA_IGNORE_PLAYER) && !HasMemory(bits_MEMORY_PROVOKED))
			return D_NU;
	}

	return BaseClass::IRelationType(pTarget);
}


//-----------------------------------------------------------------------------
// Purpose: Returns this monster's place in the relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Zombiea::Classify(void)
{
	return CLASS_ALIEN_MONSTER;
}

int AE_FASTZOMBIE_CLIMB_LEFT;
int AE_FASTZOMBIE_CLIMB_RIGHT;

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Zombiea::HandleAnimEvent(animevent_t *pEvent)
{

	if (pEvent->event == AE_FASTZOMBIE_CLIMB_LEFT || pEvent->event == AE_FASTZOMBIE_CLIMB_RIGHT)
	{


		return;
	}


	Vector v_forward, v_right;
	switch (pEvent->event)
	{
	case ZOMBIEA_AE_ATTACK_RIGHT:
	{
		// do stuff for this event.
		//		ALERT( at_console, "Slash right!\n" );

		Vector vecMins = GetHullMins();
		Vector vecMaxs = GetHullMaxs();
		vecMins.z = vecMins.x;
		vecMaxs.z = vecMaxs.x;

		CBaseEntity *pHurt = CheckTraceHullAttack(70, vecMins, vecMaxs, sk_zombiea_dmg_one_slash.GetFloat(), DMG_SLASH);
		CPASAttenuationFilter filter(this);
		if (pHurt)
		{
			if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
			{
				pHurt->ViewPunch(QAngle(5, 0, 18));

				GetVectors(&v_forward, &v_right, NULL);

				pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() - v_right * 100);
			}
			// Play a random attack hit sound
			EmitSound(filter, entindex(), "Zombie_Zomb.AttackHit");
		}
		else // Play a random attack miss sound
			EmitSound(filter, entindex(), "Zombie_Zomb.AttackMiss");

		if (random->RandomInt(0, 1))
			AttackSound();
	}
	break;

	case ZOMBIEA_AE_ATTACK_LEFT:
	{
		// do stuff for this event.
		//		ALERT( at_console, "Slash left!\n" );
		Vector vecMins = GetHullMins();
		Vector vecMaxs = GetHullMaxs();
		vecMins.z = vecMins.x;
		vecMaxs.z = vecMaxs.x;

		CBaseEntity *pHurt = CheckTraceHullAttack(70, vecMins, vecMaxs, sk_zombiea_dmg_one_slash.GetFloat(), DMG_SLASH);

		CPASAttenuationFilter filter2(this);
		if (pHurt)
		{
			if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
			{
				pHurt->ViewPunch(QAngle(5, 0, -18));

				GetVectors(&v_forward, &v_right, NULL);

				pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() - v_right * 100);
			}
			EmitSound(filter2, entindex(), "Zombie_Zomb.AttackHit");
		}
		else
		{
			EmitSound(filter2, entindex(), "Zombie_Zomb.AttackMiss");
		}

		if (random->RandomInt(0, 1))
			AttackSound();
	}
	break;

	case ZOMBIEA_AE_ATTACK_BOTH:
	{
		// do stuff for this event.
		Vector vecMins = GetHullMins();
		Vector vecMaxs = GetHullMaxs();
		vecMins.z = vecMins.x;
		vecMaxs.z = vecMaxs.x;

		CBaseEntity *pHurt = CheckTraceHullAttack(280, vecMins, vecMaxs, sk_zombiea_dmg_both_slash.GetFloat(), DMG_SLASH);


		CPASAttenuationFilter filter3(this);
		if (pHurt)
		{
			if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
			{
				pHurt->ViewPunch(QAngle(5, 0, 0));

				GetVectors(&v_forward, &v_right, NULL);
				pHurt->SetAbsVelocity(pHurt->GetAbsVelocity() - v_right * 100);
			}
			EmitSound(filter3, entindex(), "Zombie_Zomb.AttackHit");
		}
		else
			EmitSound(filter3, entindex(), "Zombie_Zomb.AttackMiss");

		if (random->RandomInt(0, 1))
			AttackSound();
	}
	break;

	default:
		BaseClass::HandleAnimEvent(pEvent);
		break;
	}
}


static float DamageForce(const Vector &size, float damage)
{
	float force = damage * ((32 * 32 * 72.0) / (size.x * size.y * size.z)) * 5;

	if (force > 1000.0)
	{
		force = 1000.0;
	}

	return force;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pInflictor - 
//			pAttacker - 
//			flDamage - 
//			bitsDamageType - 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Zombiea::OnTakeDamage_Alive(const CTakeDamageInfo &inputInfo)
{
	CTakeDamageInfo info = inputInfo;

	// Take 30% damage from bullets
	if (info.GetDamageType() == DMG_BULLET)
	{
		Vector vecDir = GetAbsOrigin() - info.GetInflictor()->WorldSpaceCenter();
		VectorNormalize(vecDir);
		float flForce = DamageForce(WorldAlignSize(), info.GetDamage());
		SetAbsVelocity(GetAbsVelocity() + vecDir * flForce);
		info.ScaleDamage(0.3f);
	}

	// HACK HACK -- until we fix this.
	if (IsAlive())
		PainSound(info);

	return BaseClass::OnTakeDamage_Alive(info);
}

void CNPC_Zombiea::OnChangeActivity(Activity NewActivity)
{


	if (NewActivity == ACT_CLIMB_UP)
	{
		// Started a climb!


		SetTouch(NULL);
	}

	else if (GetActivity() == ACT_CLIMB_DISMOUNT || (GetActivity() == ACT_CLIMB_UP && NewActivity != ACT_CLIMB_DISMOUNT))
	{
		// Ended a climb


		SetTouch(NULL);
	}

	BaseClass::OnChangeActivity(NewActivity);
}


void CNPC_Zombiea::PainSound(const CTakeDamageInfo &info)
{
	if (random->RandomInt(0, 5) < 2)
	{
		CPASAttenuationFilter filter(this);
		EmitSound(filter, entindex(), "Zombie_Zomb.Pain");
	}
}

void CNPC_Zombiea::AlertSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Zombie_Zomb.Alert");
}

void CNPC_Zombiea::IdleSound(void)
{
	// Play a random idle sound
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Zombie_Zomb.Idle");
}

void CNPC_Zombiea::AttackSound(void)
{
	// Play a random attack sound
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Zombie_Zomb.Attack");
}

int CNPC_Zombiea::MeleeAttack1Conditions(float flDot, float flDist)
{
	if (flDist > 64)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return 0;
	}
	else if (GetEnemy() == NULL)
	{
		return 0;
	}

	return COND_CAN_MELEE_ATTACK1;
}

void CNPC_Zombiea::RemoveIgnoredConditions(void)
{
	if (GetActivity() == ACT_MELEE_ATTACK1)
	{
		// Nothing stops an attacking zombie.
		ClearCondition(COND_LIGHT_DAMAGE);
		ClearCondition(COND_HEAVY_DAMAGE);
	}

	if ((GetActivity() == ACT_SMALL_FLINCH) || (GetActivity() == ACT_BIG_FLINCH))
	{
		if (m_flNextFlinch < gpGlobals->curtime)
			m_flNextFlinch = gpGlobals->curtime + ZOMBIEA_FLINCH_DELAY;
	}

	BaseClass::RemoveIgnoredConditions();
}
