#ifndef RENEGADE_COLLISION_FIX_H
#define RENEGADE_COLLISION_FIX_H

/*
** Port collision mitigations (linear static scan, AABTree brute-force fallback,
** cull-box cast/intersect fallback).  Enabled on Linux and MinGW builds.
*/
#if defined(RENEGADE_LINUX) || defined(RENEGADE_PORT_COLLISION_FIX)
#define RENEGADE_COLLISION_FIX 1
#endif

#endif /* RENEGADE_COLLISION_FIX_H */
