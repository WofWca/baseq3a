// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_utils.c -- misc utility functions for game module

#include "g_local.h"

typedef struct {
  char oldShader[MAX_QPATH];
  char newShader[MAX_QPATH];
  float timeOffset;
} shaderRemap_t;

#define MAX_SHADER_REMAPS 128

int remapCount = 0;
shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];

void AddRemap(const char *oldShader, const char *newShader, float timeOffset) {
	int i;

	for (i = 0; i < remapCount; i++) {
		if (Q_stricmp(oldShader, remappedShaders[i].oldShader) == 0) {
			// found it, just update this one
			strcpy(remappedShaders[i].newShader,newShader);
			remappedShaders[i].timeOffset = timeOffset;
			return;
		}
	}
	if (remapCount < MAX_SHADER_REMAPS) {
		strcpy(remappedShaders[remapCount].newShader,newShader);
		strcpy(remappedShaders[remapCount].oldShader,oldShader);
		remappedShaders[remapCount].timeOffset = timeOffset;
		remapCount++;
	}
}

const char *BuildShaderStateConfig(void) {
	static char	buff[MAX_STRING_CHARS*4];
	char out[(MAX_QPATH * 2) + 5];
	int i;
  
	memset( buff, 0, sizeof( buff ) );
	for (i = 0; i < remapCount; i++) {
		Com_sprintf(out, (MAX_QPATH * 2) + 5, "%s=%s:%5.2f@", remappedShaders[i].oldShader, remappedShaders[i].newShader, remappedShaders[i].timeOffset);
		Q_strcat( buff, sizeof( buff ), out);
	}
	return buff;
}

/*
=========================================================================

model / sound configstring indexes

=========================================================================
*/

/*
================
G_FindConfigstringIndex

================
*/
int G_FindConfigstringIndex( const char *name, int start, int max, qboolean create ) {
	int		i;
	char	s[MAX_STRING_CHARS];

	if ( !name || !name[0] ) {
		return 0;
	}

	for ( i=1 ; i<max ; i++ ) {
		trap_GetConfigstring( start + i, s, sizeof( s ) );
		if ( !s[0] ) {
			break;
		}
		if ( !strcmp( s, name ) ) {
			return i;
		}
	}

	if ( !create ) {
		return 0;
	}

	if ( i == max ) {
		G_Error( "G_FindConfigstringIndex: overflow" );
	}

	trap_SetConfigstring( start + i, name );

	return i;
}


int G_ModelIndex( const char *name ) {
	return G_FindConfigstringIndex (name, CS_MODELS, MAX_MODELS, qtrue);
}

int G_SoundIndex( const char *name ) {
	return G_FindConfigstringIndex (name, CS_SOUNDS, MAX_SOUNDS, qtrue);
}

//=====================================================================


/*
================
G_TeamCommand

Broadcasts a command to only a specific team
================
*/
void G_TeamCommand( team_t team, const char *cmd ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			if ( level.clients[i].sess.sessionTeam == team ) {
				trap_SendServerCommand( i, cmd );
			}
		}
	}
}


/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the entity after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match)
{
	const gentity_t *to;
	char	*s;

	if (!from)
		from = g_entities;
	else
		from++;

	to = &g_entities[ level.num_entities ];

	for ( ; from < to ; from++ )
	{
		if (!from->inuse)
			continue;
		s = *(char **) ((byte *)from + fieldofs);
		if (!s)
			continue;
		if (!Q_stricmp (s, match))
			return from;
	}

	return NULL;
}


/*
=============
G_PickTarget

Selects a random entity from among the targets
=============
*/
#define MAXCHOICES	32

gentity_t *G_PickTarget( const char *targetname )
{
	gentity_t	*choice[MAXCHOICES];
	gentity_t	*ent;
	int			num_choices;

	if (!targetname)
	{
		G_Printf("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	ent = NULL;
	num_choices = 0;
	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if ( num_choices >= MAXCHOICES )
			break;
	}

	if (!num_choices)
	{
		G_Printf("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}


/*
==============================
G_UseTargets

"activator" should be set to the entity that initiated the firing.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets( gentity_t *ent, gentity_t *activator ) {
	gentity_t		*t;
	float		f; 
	
	if ( !ent ) {
		return;
	}

	if (ent->targetShaderName && ent->targetShaderNewName) {
		f = level.time * 0.001;
		AddRemap(ent->targetShaderName, ent->targetShaderNewName, f);
		trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
	}

	if ( !ent->target ) {
		return;
	}

	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), ent->target)) != NULL ) {
		if ( t == ent ) {
			G_Printf ("WARNING: Entity used itself.\n");
		} else {
			if ( t->use ) {
				t->use (t, ent, activator);
			}
		}
		if ( !ent->inuse ) {
			G_Printf("entity was removed while using targets\n");
			return;
		}
	}
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float	*tv( float x, float y, float z ) {
	static	int		index;
	static	vec3_t	vecs[8];
	float	*v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = (index + 1)&7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}


/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char	*vtos( const vec3_t v ) {
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	Com_sprintf (s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}


/*
===============
G_SetMovedir

The editor only specifies a single value for angles (yaw),
but we have special constants to generate an up or down direction.
Angles will be cleared, because it is being used to represent a direction
instead of an orientation.
===============
*/
void G_SetMovedir( vec3_t angles, vec3_t movedir ) {
	static vec3_t VEC_UP		= {0, -1, 0};
	static vec3_t MOVEDIR_UP	= {0, 0, 1};
	static vec3_t VEC_DOWN		= {0, -2, 0};
	static vec3_t MOVEDIR_DOWN	= {0, 0, -1};

	if ( VectorCompare (angles, VEC_UP) ) {
		VectorCopy (MOVEDIR_UP, movedir);
	} else if ( VectorCompare (angles, VEC_DOWN) ) {
		VectorCopy (MOVEDIR_DOWN, movedir);
	} else {
		AngleVectors (angles, movedir, NULL, NULL);
	}
	VectorClear( angles );
}


float vectoyaw( const vec3_t vec ) {
	float	yaw;
	
	if (vec[YAW] == 0 && vec[PITCH] == 0) {
		yaw = 0;
	} else {
		if (vec[PITCH]) {
			yaw = ( atan2( vec[YAW], vec[PITCH]) * 180 / M_PI );
		} else if (vec[YAW] > 0) {
			yaw = 90;
		} else {
			yaw = 270;
		}
		if (yaw < 0) {
			yaw += 360;
		}
	}

	return yaw;
}


void G_InitGentity( gentity_t *e ) {
	e->inuse = qtrue;
	e->classname = "noclass";
	e->s.number = e - g_entities;
	e->r.ownerNum = ENTITYNUM_NONE;
	e->tag = TAG_NONE;
}


/*
=================
G_Spawn

Either finds a free entity, or allocates a new one.

  The slots from 0 to MAX_CLIENTS-1 are always reserved for clients, and will
never be used by anything else.

Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
gentity_t *G_Spawn( void ) {
	int			i, timeout;
	gentity_t	*e;

	e = NULL; // shut up warning
	// try to release oldest items first
	for ( timeout = 1000 ; timeout >= 0 ; timeout -= 250 ) {
		// if we go through all entities and can't find one to free,
		// override the normal minimum times before use
		e = &g_entities[ MAX_CLIENTS ];
		for ( i = MAX_CLIENTS ; i < level.num_entities; i++, e++ ) {
			if ( e->inuse ) {
				continue;
			}

			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if ( e->freetime > level.startTime + 2000 && level.time - e->freetime < timeout ) {
				continue;
			}

#ifndef NO_OPTIMIZED_BASELINE_ENTITY_STATE
			// Keep pool slots for the entity kinds they belong to
			// (see `G_SpawnFromEntPool`), because they have
			// matching baseline state (better delta compression).
			// Unless we're running out of regular slots,
			// in which case gameplay is more important.
			if ( e->isEntPoolSlot && timeout > 0 ) {
				continue;
			}
#endif

			// reuse this slot
			G_InitGentity( e );
			return e;
		}

		if ( level.num_entities < ENTITYNUM_MAX_NORMAL ) {
			break;
		}
	}

	if ( i == ENTITYNUM_MAX_NORMAL ) {
		for (i = 0; i < MAX_GENTITIES; i++) {
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		G_Error( "G_Spawn: no free entities" );
	}
	
	// open up a new slot
	level.num_entities++;

	// let the server system know that there are more entities
	trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	G_InitGentity( e );
	return e;
}


/*
=================
G_EntitiesFree
=================
*/
qboolean G_EntitiesFree( void ) {
	int			i;
	gentity_t	*e;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
		if ( e->inuse ) {
			continue;
		}
		// slot available
		return qtrue;
	}
	return qfalse;
}


/*
=================
G_FreeEntity

Marks the entity as free
=================
*/
void G_FreeEntity( gentity_t *ed ) {
#ifndef NO_OPTIMIZED_BASELINE_ENTITY_STATE
	qboolean	isEntPoolSlot = ed->isEntPoolSlot;
#endif

	trap_UnlinkEntity (ed);		// unlink from world

	if ( ed->neverFree ) {
		return;
	}

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = qfalse;
#ifndef NO_OPTIMIZED_BASELINE_ENTITY_STATE
	// The slot remains reserved for its pool even when free.
	ed->isEntPoolSlot = isEntPoolSlot;
#endif
}


#ifndef NO_OPTIMIZED_BASELINE_ENTITY_STATE
typedef struct {
	int		numSlots;	// must be <= ENTPOOL_MAX_SLOTS
	// The baseline values.
	int		eType;
	int		weapon;
	int		trType;
	int		eFlags;
} entPoolDef_t;

// The sizes are a tradeoff: every slot adds an entry
// to the initial `svc_gamestate` message, and a slot only pays off
// while its kind of entity actually occupies it.
// Long bursts of fire will exhaust a pool
// (also keep in mind the 1000ms reuse delay, see `G_SpawnFromEntPool`),
// after which extra entities fall back to regular slots -
// that's fine, we only lose the optimization, not the entity.
static const entPoolDef_t entPoolDefs[ENTPOOL_NUM_POOLS] = {
	// Must be in the same order as `entPool_t`.
	{ 8,	ET_MISSILE,	WP_ROCKET_LAUNCHER,		TR_LINEAR,		0 },
	{ 12,	ET_MISSILE,	WP_PLASMAGUN,			TR_LINEAR,		0 },
	{ 6,	ET_MISSILE,	WP_GRENADE_LAUNCHER,	TR_GRAVITY,		EF_BOUNCE_HALF },
	{ 2,	ET_MISSILE,	WP_BFG,					TR_LINEAR,		0 },
	// The lightning gun is by far the most frequent creator of these two,
	// hence `WP_LIGHTNING` as the baseline `weapon`
	// (see `Weapon_LightningFire`; the gauntlet also makes
	// `EV_MISSILE_HIT`, but much less often).
	{ 12,	ET_EVENTS + EV_MISSILE_MISS,		WP_LIGHTNING,	TR_STATIONARY,	0 },
	{ 6,	ET_EVENTS + EV_MISSILE_HIT,			WP_LIGHTNING,	TR_STATIONARY,	0 },
	{ 8,	ET_EVENTS + EV_BULLET_HIT_WALL,		0,				TR_STATIONARY,	0 },
	{ 6,	ET_EVENTS + EV_BULLET_HIT_FLESH,	0,				TR_STATIONARY,	0 },
	{ 2,	ET_EVENTS + EV_SHOTGUN,				0,				TR_STATIONARY,	0 },
	{ 2,	ET_EVENTS + EV_RAILTRAIL,			0,				TR_STATIONARY,	0 },
};


/*
================
EntPoolsSetBaselineState

Called by `G_InitGame`, i.e. right before `SV_CreateBaseline`.
Should be called after `G_LocateSpawnSpots`, otherwise might have no effect.
Similar to `ClientsSetBaselineState`, but for entities that get spawned
and freed all the time: missiles and frequent temp (event) entities.

Unlike clients and the body queue, these don't have dedicated
entity numbers, so on its own the baseline optimization can't apply to them.
This function preallocates pools of entity slots (see `entPoolDefs`),
sets likely baseline state on them, and `G_SpawnFromEntPool` then
prefers a matching slot when spawning such an entity,
so that most of the time the baseline actually matches.

The entities themselves only live until `SV_CreateBaseline` has run
(see `level.mustFreeEntPoolEnts`); after that only their slot numbers
(`level.entPoolNums`) and the `isEntPoolSlot` mark remain.

The pools are not exclusive: if a pool is fully occupied, its entities
fall back to `G_Spawn`, and conversely `G_Spawn` may hand out pool slots
to other entities when it is out of regular slots.
================
*/
void EntPoolsSetBaselineState( void ) {
	int			pool;
	int			i;
	qboolean	warningPrinted = qfalse;

	for ( pool = 0 ; pool < ENTPOOL_NUM_POOLS ; pool++ ) {
		const entPoolDef_t	*def = &entPoolDefs[ pool ];

		for ( i = 0 ; i < def->numSlots ; i++ ) {
			gentity_t	*ent = G_Spawn();

			level.entPoolNums[pool][i] = ent->s.number;
			ent->isEntPoolSlot = qtrue;

			// Set likely baseline state, for better delta compression.
			// Same as the constant fields set by whoever spawns this kind
			// of entity (`fire_rocket`, `G_TempEntity` + its callers, ...).
			ent->s.eType = def->eType;
			ent->s.weapon = def->weapon;
			ent->s.pos.trType = def->trType;
			ent->s.eFlags = def->eFlags;

			// The server engine runs a few frames between `G_InitGame` and
			// `SV_CreateBaseline`, and e.g. `G_RunMissile` will run on the
			// `ET_MISSILE` entities during those frames. This is harmless:
			// `clipmask` is 0 so the trace hits nothing, `pos.trDelta` is 0
			// so they don't move, and `nextthink` is 0.
			// It even re-links the entities for us.
			//
			// Except `TR_GRAVITY` entities would fall regardless of
			// `pos.trDelta`, and with `pos.trTime == 0` the fall distance
			// is computed from the beginning of server time, which would
			// send `r.currentOrigin` far below the world.
			// Setting `pos.trTime` to now limits the fall to a few units.
			// The real entities always override `pos.trTime` anyway.
			if ( def->trType == TR_GRAVITY ) {
				ent->s.pos.trTime = level.time;
			}

			// For `SV_CreateBaseline` (and for the traces mentioned above)
			// the entity needs to be linked,
			// which requires an origin inside the world.
			// See `ClientsSetBaselineState`.
			if ( level.spawnSpots[0] ) {
				VectorCopy( level.spawnSpots[0]->s.origin, ent->s.pos.trBase );
				VectorCopy( level.spawnSpots[0]->s.origin, ent->r.currentOrigin );
			}

			trap_LinkEntity( ent );
			if ( !ent->r.linked && !warningPrinted ) {
				G_Printf( S_COLOR_YELLOW "WARNING: EntPoolsSetBaselineState did not actually link the entity, delta compression will be less efficient\n" );
				warningPrinted = qtrue;
			}

			// We've linked the entity, but let's not send it to clients.
			ent->r.svFlags = SVF_NOCLIENT;
		}
	}

	// Free the entities (making the pool slots available) ASAP
	// after `SV_CreateBaseline` has run.
	level.mustFreeEntPoolEnts = qtrue;

	level.entPoolsInitialized = qtrue;
}


/*
================
EntPoolsFreeEnts

Frees the entities created by `EntPoolsSetBaselineState`.
To be called once `SV_CreateBaseline` has run,
see `level.mustFreeEntPoolEnts`.
================
*/
void EntPoolsFreeEnts( void ) {
	int		pool;
	int		i;

	for ( pool = 0 ; pool < ENTPOOL_NUM_POOLS ; pool++ ) {
		for ( i = 0 ; i < entPoolDefs[pool].numSlots ; i++ ) {
			G_FreeEntity( &g_entities[ level.entPoolNums[pool][i] ] );
		}
	}
}


/*
================
G_SpawnFromEntPool

Same as `G_Spawn`, but first tries to take a slot from the given pool,
because those slots have matching baseline state,
which makes for better delta compression.
See `EntPoolsSetBaselineState`.
================
*/
gentity_t *G_SpawnFromEntPool( entPool_t pool ) {
	int			i;
	gentity_t	*e;

	// Guard against being called before the pools are set up,
	// in which case `level.entPoolNums` would point at client slots (0).
	if ( !level.entPoolsInitialized ) {
		return G_Spawn();
	}

	for ( i = 0 ; i < entPoolDefs[pool].numSlots ; i++ ) {
		e = &g_entities[ level.entPoolNums[pool][i] ];
		// Note that this is also qtrue for all the slots until
		// `level.mustFreeEntPoolEnts` gets handled,
		// but that only lasts until the first `ClientConnect`.
		if ( e->inuse ) {
			continue;
		}

		// Same policy as in `G_Spawn`: avoid reusing an entity
		// that was recently freed.
		if ( e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000 ) {
			continue;
		}

		G_InitGentity( e );
		return e;
	}

	// The whole pool is in use.
	return G_Spawn();
}
#endif


/*
=================
G_TempEntity

Spawns an event entity that will be auto-removed
The origin will be snapped to save net bandwidth, so care
must be taken if the origin is right on a surface (snap towards start vector first)
=================
*/
gentity_t *G_TempEntity( vec3_t origin, int event ) {
	gentity_t		*e;
	vec3_t		snapped;

#ifndef NO_OPTIMIZED_BASELINE_ENTITY_STATE
	// The most frequent events have entity pools with matching
	// baseline state, for better delta compression.
	// See `EntPoolsSetBaselineState`.
	switch ( event ) {
	case EV_MISSILE_MISS:
		e = G_SpawnFromEntPool( ENTPOOL_EV_MISSILE_MISS );
		break;
	case EV_MISSILE_HIT:
		e = G_SpawnFromEntPool( ENTPOOL_EV_MISSILE_HIT );
		break;
	case EV_BULLET_HIT_WALL:
		e = G_SpawnFromEntPool( ENTPOOL_EV_BULLET_HIT_WALL );
		break;
	case EV_BULLET_HIT_FLESH:
		e = G_SpawnFromEntPool( ENTPOOL_EV_BULLET_HIT_FLESH );
		break;
	case EV_SHOTGUN:
		e = G_SpawnFromEntPool( ENTPOOL_EV_SHOTGUN );
		break;
	case EV_RAILTRAIL:
		e = G_SpawnFromEntPool( ENTPOOL_EV_RAILTRAIL );
		break;
	default:
		e = G_Spawn();
		break;
	}
#else
	e = G_Spawn();
#endif
	e->s.eType = ET_EVENTS + event;

	e->classname = "tempEntity";
	e->eventTime = level.time;
	e->freeAfterEvent = qtrue;

	VectorCopy( origin, snapped );
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( e, snapped );

	// find cluster for PVS
	trap_LinkEntity( e );

	return e;
}



/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
G_EntitiesInBox
=================
*/
int G_EntitiesInBox( vec3_t mins, vec3_t maxs, int *touch, int maxents ) {
	const gentity_t *ent;
	int i, count;

	count = 0;
	ent = g_entities;

	for ( i = 0; i < maxents; i++, ent++ ) {
		if ( !ent->r.linked || !ent->inuse ) {
			continue;
		}
		if ( ent->r.absmin[0] > maxs[0] ||
			ent->r.absmin[1] > maxs[1] ||
			ent->r.absmin[2] > maxs[2] ||
			ent->r.absmax[0] < mins[0] ||
			ent->r.absmax[1] < mins[1] ||
			ent->r.absmax[2] < mins[2] ) {
			continue;
		}
		touch[ count ] = i;
		count++;
	}

	return count;
}

/*
=================
G_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
void G_KillBox( gentity_t *ent ) {
	int			i, num;
	int			touch[MAX_CLIENTS];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );
	num = G_EntitiesInBox( mins, maxs, touch, level.maxclients );

	for ( i = 0; i < num; i++ ) {
		hit = &g_entities[touch[i]];
		if ( !hit->client ) {
			continue;
		}

		// nail it
		G_Damage( hit, ent, ent, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG );
	}
}

//==============================================================================

/*
===============
G_AddPredictableEvent

Use for non-pmove events that would also be predicted on the
client side: jumppads and item pickups
Adds an event+parm and twiddles the event counter
===============
*/
void G_AddPredictableEvent( gentity_t *ent, entity_event_t event, int eventParm ) {
	if ( !ent->client ) {
		return;
	}
	BG_AddPredictableEventToPlayerstate( event, eventParm, &ent->client->ps, -1 );
}


/*
===============
G_AddEvent

Adds an event+parm and twiddles the event counter
===============
*/
void G_AddEvent( gentity_t *ent, int event, int eventParm ) {
	int		bits;
	gclient_t *client;

	if ( !event ) {
		G_Printf( "G_AddEvent: zero event added for entity %i\n", ent->s.number );
		return;
	}

	// clients need to add the event in playerState_t instead of entityState_t
	if ( ent->client ) {
		client = ent->client;
		bits = client->ps.externalEvent & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		client->ps.externalEvent = event | bits;
		client->ps.externalEventParm = eventParm;
		client->ps.externalEventTime = level.time;
	} else {
		bits = ent->s.event & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		ent->s.event = event | bits;
		ent->s.eventParm = eventParm;
	}
	ent->eventTime = level.time;
}


/*
=============
G_Sound
=============
*/
void G_Sound( gentity_t *ent, int channel, int soundIndex ) {
	gentity_t	*te;

	te = G_TempEntity( ent->r.currentOrigin, EV_GENERAL_SOUND );
	te->s.eventParm = soundIndex;
}


//==============================================================================


/*
================
G_SetOrigin

Sets the pos trajectory for a fixed position
================
*/
void G_SetOrigin( gentity_t *ent, vec3_t origin ) {
	VectorCopy( origin, ent->s.pos.trBase );
	ent->s.pos.trType = TR_STATIONARY;
	ent->s.pos.trTime = 0;
	ent->s.pos.trDuration = 0;
	VectorClear( ent->s.pos.trDelta );

	VectorCopy( origin, ent->r.currentOrigin );
}

/*
================
DebugLine

  debug polygons only work when running a local game
  with r_debugSurface set to 2
================
*/
int DebugLine(vec3_t start, vec3_t end, int color) {
	vec3_t points[4], dir, cross, up = {0, 0, 1};
	float dot;

	VectorCopy(start, points[0]);
	VectorCopy(start, points[1]);
	//points[1][2] -= 2;
	VectorCopy(end, points[2]);
	//points[2][2] -= 2;
	VectorCopy(end, points[3]);


	VectorSubtract(end, start, dir);
	VectorNormalize(dir);
	dot = DotProduct(dir, up);
	if (dot > 0.99 || dot < -0.99) VectorSet(cross, 1, 0, 0);
	else CrossProduct(dir, up, cross);

	VectorNormalize(cross);

	VectorMA(points[0], 2, cross, points[0]);
	VectorMA(points[1], -2, cross, points[1]);
	VectorMA(points[2], -2, cross, points[2]);
	VectorMA(points[3], 2, cross, points[3]);

	return trap_DebugPolygonCreate(color, 4, points);
}
