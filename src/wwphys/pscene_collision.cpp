/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WWPhys                                                       *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwphys/pscene_collision.cpp                  $*
 *                                                                                             *
 *              Original Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                      $Author:: Greg_h                                                      $*
 *                                                                                             *
 *                     $Modtime:: 2/13/02 2:16p                                               $*
 *                                                                                             *
 *                    $Revision:: 10                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "pscene.h"
#include "physcoltest.h"
#include "physinttest.h"
#include "staticaabtreecull.h"
#include "dynamicaabtreecull.h"
#include "physgridcull.h"
#include "lightcull.h"
#include "staticphys.h"
#include "colmathaabox.h"
#include "colmath.h"
#include "wwmath.h"
#include "coltype.h"
#include "aabox.h"
#include "staticphys.h"
#include "renegade_collision_fix.h"


#if defined(RENEGADE_COLLISION_FIX)

static void
Linux_Pad_Query_AABox ( AABoxClass &bounds, float min_extent )
{
	if ( bounds.Extent.X < min_extent ) {
		bounds.Extent.X = min_extent;
	}
	if ( bounds.Extent.Y < min_extent ) {
		bounds.Extent.Y = min_extent;
	}
	if ( bounds.Extent.Z < min_extent ) {
		bounds.Extent.Z = min_extent;
	}
}

/*
** Reject codes for vehicle cull-intersect fallback (0 = allowed).
*/
static int
Linux_Vehicle_Cull_Intersect_Reject_Code
(
	PhysClass *obj,
	const OBBoxClass &query_box
)
{
	//
	// Allow compact props only; buildings use pathfind / mesh collision.
	//
	const AABoxClass &cull = obj->Get_Cull_Box ();
	float min_ext = cull.Extent.X;
	float max_ext = min_ext;
	if ( cull.Extent.Y < min_ext ) {
		min_ext = cull.Extent.Y;
	}
	if ( cull.Extent.Z < min_ext ) {
		min_ext = cull.Extent.Z;
	}
	if ( cull.Extent.Y > max_ext ) {
		max_ext = cull.Extent.Y;
	}
	if ( cull.Extent.Z > max_ext ) {
		max_ext = cull.Extent.Z;
	}

	RenderObjClass *model = obj->Peek_Model ();
	if ( model == NULL ) {
		return 3;
	}

	int class_id = model->Class_ID ();
	if ( class_id != RenderObjClass::CLASSID_MESH && class_id != RenderObjClass::CLASSID_HLOD ) {
		return 4;
	}

	//
	// Match StaticPhysClass::Cast_OBBox cull fallback (0.25-10 m).
	// Buildings >10 m are handled by mesh-level brute_force collision.
	//
	if ( max_ext < 0.25f || max_ext > 10.0f ) {
		return 2;
	}

	if ( min_ext < 0.35f && max_ext > 2.0f ) {
		return 5;
	}

	float max_query_ext = query_box.Extent.X;
	if ( query_box.Extent.Y > max_query_ext ) {
		max_query_ext = query_box.Extent.Y;
	}
	if ( query_box.Extent.Z > max_query_ext ) {
		max_query_ext = query_box.Extent.Z;
	}

	//
	// Tall vertical culls (lamppost): side overlap resolves upward — block
	// intersect fallback; movement blocking still uses cull cast in staticphys.
	//
	float horiz_ext = cull.Extent.X;
	if ( cull.Extent.Y > horiz_ext ) {
		horiz_ext = cull.Extent.Y;
	}
	if ( horiz_ext > 0.001f && cull.Extent.Z > horiz_ext * 1.8f &&
			query_box.Center.Z < cull.Center.Z - horiz_ext * 0.5f ) {
		return 8;
	}

	float cull_top = cull.Center.Z + cull.Extent.Z;
	float query_bottom = query_box.Center.Z - max_query_ext;
	if ( query_bottom > cull_top - 0.20f ) {
		return 6;
	}

	return 0;
}

static bool
Linux_Vehicle_Cull_Intersect_Allowed ( PhysClass *obj, const OBBoxClass &query_box )
{
	return Linux_Vehicle_Cull_Intersect_Reject_Code ( obj, query_box ) == 0;
}

#if defined(RENEGADE_LINUX)
/*
** Vertical weather/terrain rays collapse to zero-thickness AABoxes in XY; overlap
** tests against scene nodes then reject everything (Collect_Objects returned 0).
*/
static void
Linux_Init_Ray_Query_Bounds ( const LineSegClass &ray, AABoxClass &out_bounds )
{
	out_bounds.Init ( ray );
	Linux_Pad_Query_AABox ( out_bounds, 1.0f );
}
#endif

bool
PhysicsSceneClass::Linux_Intersect_Static_In_AABox
(
	const AABoxClass &query_bounds,
	int collision_group,
	PhysAABoxIntersectionTestClass &boxtest
)
{
	RefPhysListIterator it ( &StaticObjList );
	for ( it.First (); !it.Is_Done (); it.Next () ) {
		PhysClass *obj = it.Peek_Obj ();
		if (	Do_Groups_Collide ( obj->Get_Collision_Group (), collision_group ) &&
				!obj->Is_Ignore_Me () &&
				CollisionMath::Overlap_Test ( query_bounds, obj->Get_Cull_Box () ) != CollisionMath::OUTSIDE )
		{
			if ( obj->Intersection_Test ( boxtest ) ) {
				return true;
			}
		}
	}
	return false;
}


bool
PhysicsSceneClass::Linux_Intersect_Static_In_AABox
(
	const AABoxClass &query_bounds,
	int collision_group,
	PhysOBBoxIntersectionTestClass &boxtest
)
{
	const bool vehicle_query = ( boxtest.CollisionType & COLLISION_TYPE_VEHICLE ) != 0;

	RefPhysListIterator it ( &StaticObjList );
	for ( it.First (); !it.Is_Done (); it.Next () ) {
		PhysClass *obj = it.Peek_Obj ();
		if ( CollisionMath::Overlap_Test ( query_bounds, obj->Get_Cull_Box () ) != CollisionMath::OUTSIDE ) {
			if ( !Do_Groups_Collide ( obj->Get_Collision_Group (), collision_group ) ) {
				continue;
			}
			if ( obj->Is_Ignore_Me () ) {
				continue;
			}
			bool cull_intersects = CollisionMath::Intersection_Test ( obj->Get_Cull_Box (), boxtest.Box );
			if ( obj->Intersection_Test ( boxtest ) ) {
				return true;
			}
			if ( vehicle_query && cull_intersects ) {
				int reject = Linux_Vehicle_Cull_Intersect_Reject_Code ( obj, boxtest.Box );
				if ( reject == 0 ) {
					return true;
				}
			}
		}
	}

	return false;
}


void
PhysicsSceneClass::Linux_Collect_Static_In_AABox
(
	const AABoxClass &query_bounds,
	int collision_group,
	NonRefPhysListClass *list
)
{
	RefPhysListIterator it ( &StaticObjList );
	for ( it.First (); !it.Is_Done (); it.Next () ) {
		PhysClass *obj = it.Peek_Obj ();
		if (	Do_Groups_Collide ( obj->Get_Collision_Group (), collision_group ) &&
				!obj->Is_Ignore_Me () &&
				CollisionMath::Overlap_Test ( query_bounds, obj->Get_Cull_Box () ) != CollisionMath::OUTSIDE )
		{
			list->Add ( obj );
		}
	}
}

void
PhysicsSceneClass::Linux_Collect_Static_In_AABox_All
(
	const AABoxClass &query_bounds,
	NonRefPhysListClass *list
)
{
	AABoxClass bounds = query_bounds;
	Linux_Pad_Query_AABox ( bounds, 0.5f );

	RefPhysListIterator it ( &StaticObjList );
	for ( it.First (); !it.Is_Done (); it.Next () ) {
		PhysClass *obj = it.Peek_Obj ();
		if ( CollisionMath::Overlap_Test ( bounds, obj->Get_Cull_Box () ) != CollisionMath::OUTSIDE ) {
			list->Add ( obj );
		}
	}
}
#endif


bool PhysicsSceneClass::Do_Groups_Collide(int group0,int group1)
{
	int index = group0 | (group1 << COLLISION_FLAG_SHIFT);
	return AllowCollisionFlags[index];
}


void PhysicsSceneClass::Enable_Collision_Detection(int group0,int group1)
{
	assert(group0 >= 0);
	assert(group1 >= 0);
	assert(group0 <= MAX_COLLISION_GROUP);
	assert(group1 <= MAX_COLLISION_GROUP);

	unsigned int index;

	index = group0 | (group1 << COLLISION_FLAG_SHIFT);
	AllowCollisionFlags[index] = 1;

	index = group1 | (group0 << COLLISION_FLAG_SHIFT);
	AllowCollisionFlags[index] = 1;
}

void PhysicsSceneClass::Disable_Collision_Detection(int group0,int group1)
{
	assert(group0 >= 0);
	assert(group1 >= 0);
	assert(group0 <= MAX_COLLISION_GROUP);
	assert(group1 <= MAX_COLLISION_GROUP);

	unsigned int index;

	index = group0 | (group1 << COLLISION_FLAG_SHIFT);
	AllowCollisionFlags[index] = 0;

	index = group1 | (group0 << COLLISION_FLAG_SHIFT);
	AllowCollisionFlags[index] = 0;
}

void PhysicsSceneClass::Enable_All_Collision_Detections(int group)
{
	assert(group >= 0);
	assert(group <= MAX_COLLISION_GROUP);

	for (int i=0; i <= MAX_COLLISION_GROUP; i++) {

		unsigned int index;
		index = group | (i << COLLISION_FLAG_SHIFT);
		AllowCollisionFlags[index] = 1;

		index = i | (group << COLLISION_FLAG_SHIFT);
		AllowCollisionFlags[index] = 1;
	}
}


void PhysicsSceneClass::Disable_All_Collision_Detections(int group)
{
	assert(group >= 0);
	assert(group <= MAX_COLLISION_GROUP);

	for (int i=0; i <= MAX_COLLISION_GROUP; i++) {

		unsigned int index;
		index = group | (i << COLLISION_FLAG_SHIFT);
		AllowCollisionFlags[index] = 0;

		index = i | (group << COLLISION_FLAG_SHIFT);
		AllowCollisionFlags[index] = 0;
	}
}

void PhysicsSceneClass::Set_Collision_Region(const AABoxClass & bounds,int colgroup)
{
	Collect_Collideable_Objects(bounds,colgroup,true,true,&CollisionRegionList);
}

void PhysicsSceneClass::Release_Collision_Region(void)
{
	CollisionRegionList.Reset_List();
}

bool PhysicsSceneClass::Cast_Ray(PhysRayCollisionTestClass & raytest,bool use_collision_region)
{
	/*
	** Assert that the result structure has been initialized with the 
	** optimistic result that the entire move will be taken.  Each call 
	** should whittle down the Fraction variable so that we are left 
	** with the fraction that the closest object allowed.  If StartBad
	** is ever set to true, we can bail out.
	*/
	assert(raytest.Result->Fraction == 1.0f);
	assert(raytest.Result->StartBad == false);
	raytest.CollidedPhysObj = NULL;

	/*
	** Check against physical objects in our vicinity
	*/
	bool res = false;

	if (use_collision_region) {

		/*
		** Use the cached collision region list
		*/
		NonRefPhysListIterator it(&CollisionRegionList);
		for ( ; !it.Is_Done(); it.Next()) {
			PhysClass * obj = it.Peek_Obj();
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),raytest.CollisionGroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				res |= obj->Cast_Ray(raytest);
			}
		}
	
	} else {

		/*
		** Cull the collision check using the culling systems
		*/
		if (raytest.CheckStaticObjs) {
#if defined(RENEGADE_LINUX)
			/*
			** LP64 AABTree raycast misses terrain; use padded-bounds linear prefilter.
			*/
			res = false;
#else
			res |= StaticCullingSystem->Cast_Ray(raytest);
			if (raytest.Result->StartBad) return true;
#endif

#if defined(RENEGADE_COLLISION_FIX)
			//
			//	The tree may miss geometry or return the wrong fraction.
			//	Always run the linear scan so the nearest hit wins.
			//
#if defined(RENEGADE_LINUX)
			AABoxClass ray_bounds;
			Linux_Init_Ray_Query_Bounds ( raytest.Ray, ray_bounds );
#endif
			RefPhysListIterator it ( &StaticObjList );
			for ( it.First (); !it.Is_Done (); it.Next () ) {
				PhysClass *obj = it.Peek_Obj ();
				if (	Do_Groups_Collide ( obj->Get_Collision_Group (), raytest.CollisionGroup ) &&
						!obj->Is_Ignore_Me () )
				{
#if defined(RENEGADE_LINUX)
					if ( CollisionMath::Overlap_Test ( ray_bounds, obj->Get_Cull_Box () ) == CollisionMath::OUTSIDE ) {
						continue;
					}
#endif
					res |= obj->Cast_Ray ( raytest );
					if ( raytest.Result->StartBad ) {
						return true;
					}
				}
			}
#endif
		}
		
		if (raytest.CheckDynamicObjs) {
			res |= DynamicCullingSystem->Cast_Ray(raytest);
			if (raytest.Result->StartBad) return true;
		}
	
	} 

	return res;
}

bool PhysicsSceneClass::Cast_AABox(PhysAABoxCollisionTestClass & boxtest,bool use_collision_region)
{
	/*
	** Assert that the result structure has been initialized with the 
	** optimistic result that the entire move will be taken.  Each call 
	** should whittle down the Fraction variable so that we are left 
	** with the fraction that the closest object allowed.  If StartBad
	** is ever set to true, we can bail out.
	*/
	WWASSERT(boxtest.Result->Fraction == 1.0f);
	WWASSERT(boxtest.Result->StartBad == false);
	boxtest.CollidedPhysObj = NULL;
	
	/*
	** Check against physical objects in our vicinity
	*/
	bool res = false;

	if (use_collision_region) {

		/*
		** Use the cached collision region list
		*/
		NonRefPhysListIterator it(&CollisionRegionList);
		for ( ; !it.Is_Done(); it.Next()) {
			PhysClass * obj = it.Peek_Obj();
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),boxtest.CollisionGroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				res |= obj->Cast_AABox(boxtest);
			}
		}
	
	} else {	

		/*
		** Cull the collision check using the culling systems
		*/
		if (boxtest.CheckStaticObjs) {
#if defined(RENEGADE_LINUX)
			/*
			** LP64 AABTree sweeps can miss walls or report the wrong fraction.
			** Use the linear cull-box prefilter (same pattern as Cast_Ray).
			*/
			res = false;
#else
			res |= StaticCullingSystem->Cast_AABox(boxtest);
			if (boxtest.Result->StartBad) return true;
#endif

#if defined(RENEGADE_COLLISION_FIX)
			//
			//	The tree may have missed a closer obstacle even when it
			//	returned a hit (res == true).  Always run the linear scan
			//	so the nearest fraction wins.
			//
			if ( boxtest.Move.Length2 () > WWMATH_EPSILON ) {
				AABoxClass sweep_bounds;
				sweep_bounds.Init_Min_Max ( boxtest.SweepMin, boxtest.SweepMax );

				RefPhysListIterator it ( &StaticObjList );
				for ( it.First (); !it.Is_Done (); it.Next () ) {
					PhysClass *obj = it.Peek_Obj ();
					if (	Do_Groups_Collide ( obj->Get_Collision_Group (), boxtest.CollisionGroup ) &&
							!obj->Is_Ignore_Me () &&
							CollisionMath::Overlap_Test ( sweep_bounds, obj->Get_Cull_Box () ) != CollisionMath::OUTSIDE )
					{
						res |= obj->Cast_AABox ( boxtest );
						if ( boxtest.Result->StartBad ) {
							return true;
						}
					}
				}
			}
#endif
		}
		
		if (boxtest.CheckDynamicObjs) {
			res |= DynamicCullingSystem->Cast_AABox(boxtest);
			if (boxtest.Result->StartBad) return true;
		}
	}

	return res;
}

bool PhysicsSceneClass::Cast_OBBox(PhysOBBoxCollisionTestClass & boxtest,bool use_collision_region)
{
	/*
	** Assert that the result structure has been initialized with the 
	** optimistic result that the entire move will be taken.  Each call 
	** should whittle down the Fraction variable so that we are left 
	** with the fraction that the closest object allowed.  If StartBad
	** is ever set to true, we can bail out.
	*/
	assert(boxtest.Result->Fraction == 1.0f);
	assert(boxtest.Result->StartBad == false);
	boxtest.CollidedPhysObj = NULL;
	
	/*
	** Check against physical objects in our vicinity
	*/
	bool res = false;

	if (use_collision_region) {

		/*
		** Use the cached collision region list
		*/
		NonRefPhysListIterator it(&CollisionRegionList);
		for ( ; !it.Is_Done(); it.Next()) {
			PhysClass * obj = it.Peek_Obj();
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),boxtest.CollisionGroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				res |= obj->Cast_OBBox(boxtest);
			}
		}

#if defined(RENEGADE_COLLISION_FIX)
		/*
		** Collision-region casts can miss statics on LP64; fall back to the
		** full static list sweep (same as the non-region path).
		*/
		if ( boxtest.CheckStaticObjs && boxtest.Move.Length2 () > WWMATH_EPSILON ) {
			AABoxClass sweep_bounds;
			sweep_bounds.Init_Min_Max ( boxtest.SweepMin, boxtest.SweepMax );

			RefPhysListIterator sit ( &StaticObjList );
			for ( sit.First (); !sit.Is_Done (); sit.Next () ) {
				PhysClass *obj = sit.Peek_Obj ();
				if (	Do_Groups_Collide ( obj->Get_Collision_Group (), boxtest.CollisionGroup ) &&
						!obj->Is_Ignore_Me () &&
						CollisionMath::Overlap_Test ( sweep_bounds, obj->Get_Cull_Box () ) != CollisionMath::OUTSIDE )
				{
					res |= obj->Cast_OBBox ( boxtest );
					if ( boxtest.Result->StartBad ) {
						return true;
					}
				}
			}
		}
#endif
	
	} else {	

		/*
		** Cull the collision check using the culling systems
		*/
		if (boxtest.CheckStaticObjs) {
#if defined(RENEGADE_LINUX)
			res = false;
#else
			res |= StaticCullingSystem->Cast_OBBox(boxtest);
			if (boxtest.Result->StartBad) return true;
#endif

#if defined(RENEGADE_COLLISION_FIX)
			//
			//	The tree may have missed a closer obstacle even when it
			//	returned a hit (res == true).  Always run the linear scan
			//	so the nearest fraction wins.
			//
			if ( boxtest.Move.Length2 () > WWMATH_EPSILON ) {
				AABoxClass sweep_bounds;
				sweep_bounds.Init_Min_Max ( boxtest.SweepMin, boxtest.SweepMax );

				RefPhysListIterator it ( &StaticObjList );
				for ( it.First (); !it.Is_Done (); it.Next () ) {
					PhysClass *obj = it.Peek_Obj ();
					if (	Do_Groups_Collide ( obj->Get_Collision_Group (), boxtest.CollisionGroup ) &&
							!obj->Is_Ignore_Me () &&
							CollisionMath::Overlap_Test ( sweep_bounds, obj->Get_Cull_Box () ) != CollisionMath::OUTSIDE )
					{
						res |= obj->Cast_OBBox ( boxtest );
						if ( boxtest.Result->StartBad ) {
							return true;
						}
					}
				}
			}
#endif
		}
		
		if (boxtest.CheckDynamicObjs) {
			res |= DynamicCullingSystem->Cast_OBBox(boxtest);
			if (boxtest.Result->StartBad) return true;
		}
	}

	return res;
}

bool PhysicsSceneClass::Intersection_Test(PhysAABoxIntersectionTestClass & boxtest,bool use_collision_region)
{
	if (use_collision_region) {
	
		/*
		** Test for intersection with objects in the cached collision region
		*/
		NonRefPhysListIterator it(&CollisionRegionList);
		for ( ; !it.Is_Done(); it.Next()) {
			PhysClass * obj = it.Peek_Obj();
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),boxtest.CollisionGroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				if (obj->Intersection_Test(boxtest)) {
					return true;
				}
			}
		}

#if defined(RENEGADE_COLLISION_FIX)
		if ( boxtest.CheckStaticObjs ) {
			if ( Linux_Intersect_Static_In_AABox ( boxtest.Box, boxtest.CollisionGroup, boxtest ) ) {
				return true;
			}
		}
#endif
		
	} else {

		/*
		** Test for intersection with objects in the static and dynamic culling systems
		*/
		if (boxtest.CheckStaticObjs) {
			if (StaticCullingSystem->Intersection_Test(boxtest)) {
				return true;
			}
#if defined(RENEGADE_COLLISION_FIX)
			if ( Linux_Intersect_Static_In_AABox ( boxtest.Box, boxtest.CollisionGroup, boxtest ) ) {
				return true;
			}
#endif
		}
		if (boxtest.CheckDynamicObjs) {
			if (DynamicCullingSystem->Intersection_Test(boxtest)) {
				return true;
			}
		}
	
	}

	return false;
}

bool PhysicsSceneClass::Intersection_Test(PhysOBBoxIntersectionTestClass & boxtest,bool use_collision_region)
{
	if (use_collision_region) {
		
		/*
		** Test for intersection with objects in the cached collision region
		*/
		NonRefPhysListIterator it(&CollisionRegionList);
		for ( ; !it.Is_Done(); it.Next()) {
			PhysClass * obj = it.Peek_Obj();
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),boxtest.CollisionGroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				if (obj->Intersection_Test(boxtest)) {
					return true;
				}
			}
		}

#if defined(RENEGADE_COLLISION_FIX)
		if ( boxtest.CheckStaticObjs ) {
			if ( Linux_Intersect_Static_In_AABox ( boxtest.BoundingBox, boxtest.CollisionGroup, boxtest ) ) {
				return true;
			}
		}
#endif
	
	} else {

		/*
		** Test for intersection with objects in the static and dynamic culling systems
		*/
		if (boxtest.CheckStaticObjs) {
			if (StaticCullingSystem->Intersection_Test(boxtest)) {
				return true;
			}
#if defined(RENEGADE_COLLISION_FIX)
			if ( Linux_Intersect_Static_In_AABox ( boxtest.BoundingBox, boxtest.CollisionGroup, boxtest ) ) {
				return true;
			}
#endif
		}
		if (boxtest.CheckDynamicObjs) {
			if (DynamicCullingSystem->Intersection_Test(boxtest)) {
				return true;
			}
		}
	}

	return false;
}

bool PhysicsSceneClass::Intersection_Test(PhysMeshIntersectionTestClass & meshtest,bool use_collision_region)
{
	if (use_collision_region) {
		
		/*
		** Test for intersection with objects in the cached collision region
		*/
		NonRefPhysListIterator it(&CollisionRegionList);
		for ( ; !it.Is_Done(); it.Next()) {
			PhysClass * obj = it.Peek_Obj();
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),meshtest.CollisionGroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				if (obj->Intersection_Test(meshtest)) {
					return true;
				}
			}
		}
	
	} else {

		if (meshtest.CheckStaticObjs) {
			if (StaticCullingSystem->Intersection_Test(meshtest)) {
				return true;
			}
		}
		if (meshtest.CheckDynamicObjs) {
			if (DynamicCullingSystem->Intersection_Test(meshtest)) {
				return true;
			}
		}

	}
	return false;
}

bool PhysicsSceneClass::Intersection_Test(const AABoxClass & box,int collision_group,int collision_type,bool use_collision_region)
{
	NonRefPhysListClass intersect_list;
	PhysAABoxIntersectionTestClass test(box,collision_group,collision_type,&intersect_list);
	return Intersection_Test(test,use_collision_region);
}

bool PhysicsSceneClass::Intersection_Test(const OBBoxClass & box,int collision_group,int collision_type,bool use_collision_region)
{
	NonRefPhysListClass intersect_list;
	PhysOBBoxIntersectionTestClass test(box,collision_group,collision_type,&intersect_list);
	return Intersection_Test(test,use_collision_region);
}

void PhysicsSceneClass::Add_Collected_Objects_To_List
(
	bool static_objs,
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	// link the static objects
	if (static_objs) {
		StaticPhysClass * obj;
		for (	obj = (StaticPhysClass *)StaticCullingSystem->Get_First_Collected_Object();
				obj != NULL;
				obj = (StaticPhysClass *)StaticCullingSystem->Get_Next_Collected_Object(obj) )
		{
			list->Add(obj);
		}
	}

	// link the dynamic objects
	if (dynamic_objs) {
		PhysClass * obj;
		for (	obj = (PhysClass *)DynamicCullingSystem->Get_First_Collected_Object();
				obj != NULL;
				obj = (PhysClass *)DynamicCullingSystem->Get_Next_Collected_Object(obj) )
		{
			list->Add(obj);
		}
	}
}


void PhysicsSceneClass::Add_Collected_Collideable_Objects_To_List
(
	int colgroup,
	bool static_objs,
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	// link the static objects
	if (static_objs) {
		StaticPhysClass * obj;
		for (	obj = (StaticPhysClass *)StaticCullingSystem->Get_First_Collected_Object();
				obj != NULL;
				obj = (StaticPhysClass *)StaticCullingSystem->Get_Next_Collected_Object(obj) )
		{
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),colgroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				list->Add(obj);
			}
		}
	}

	// link the dynamic objects
	if (dynamic_objs) {
		PhysClass * obj;
		for (	obj = (PhysClass *)DynamicCullingSystem->Get_First_Collected_Object();
				obj != NULL;
				obj = (PhysClass *)DynamicCullingSystem->Get_Next_Collected_Object(obj) )
		{
			if (	Do_Groups_Collide(obj->Get_Collision_Group(),colgroup) && 
					!obj->Is_Ignore_Me()	) 
			{
				list->Add(obj);
			}
		}
	}
}

void PhysicsSceneClass::Collect_Objects
(
	const Vector3 & point,
	bool static_objs, 
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);

	if (static_objs) {
		StaticCullingSystem->Reset_Collection();
		StaticCullingSystem->Collect_Objects(point);
	}

	if (dynamic_objs) {
		DynamicCullingSystem->Reset_Collection();
		DynamicCullingSystem->Collect_Objects(point);
	}

	Add_Collected_Objects_To_List(static_objs,dynamic_objs,list);
}

void PhysicsSceneClass::Collect_Objects
(
	const AABoxClass & box,
	bool static_objs, 
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);

#if defined(RENEGADE_COLLISION_FIX)
	if (static_objs) {
		Linux_Collect_Static_In_AABox_All ( box, list );
	}
#else
	if (static_objs) {
		StaticCullingSystem->Reset_Collection();
		StaticCullingSystem->Collect_Objects(box);
	}
#endif

	if (dynamic_objs) {
		DynamicCullingSystem->Reset_Collection();
		DynamicCullingSystem->Collect_Objects(box);
	}

#if defined(RENEGADE_COLLISION_FIX)
	if (dynamic_objs) {
		PhysClass *obj = NULL;
		for (	obj = DynamicCullingSystem->Get_First_Collected_Object ();
				obj != NULL;
				obj = DynamicCullingSystem->Get_Next_Collected_Object ( obj ) )
		{
			list->Add ( obj );
		}
	}
#else
	Add_Collected_Objects_To_List(static_objs,dynamic_objs,list);
#endif
}

void PhysicsSceneClass::Collect_Objects
(
	const OBBoxClass & box,
	bool static_objs, 
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);

#if defined(RENEGADE_COLLISION_FIX)
	if (static_objs) {
		AABoxClass query_bounds;
		query_bounds.Center = box.Center;
		box.Compute_Axis_Aligned_Extent ( &query_bounds.Extent );
		Linux_Collect_Static_In_AABox_All ( query_bounds, list );
	}
#else
	if (static_objs) {
		StaticCullingSystem->Reset_Collection();
		StaticCullingSystem->Collect_Objects(box);
	}
#endif

	if (dynamic_objs) {
		DynamicCullingSystem->Reset_Collection();
		DynamicCullingSystem->Collect_Objects(box);
	}

#if defined(RENEGADE_COLLISION_FIX)
	if (dynamic_objs) {
		PhysClass *obj = NULL;
		for (	obj = DynamicCullingSystem->Get_First_Collected_Object ();
				obj != NULL;
				obj = DynamicCullingSystem->Get_Next_Collected_Object ( obj ) )
		{
			list->Add ( obj );
		}
	}
#else
	Add_Collected_Objects_To_List(static_objs,dynamic_objs,list);
#endif
}

void PhysicsSceneClass::Collect_Objects
(
	const FrustumClass & frustum,
	bool static_objs, 
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);
	if (static_objs) {
		StaticCullingSystem->Reset_Collection();
		StaticCullingSystem->Collect_Objects(frustum);
	}

	if (dynamic_objs) {
		DynamicCullingSystem->Reset_Collection();
		DynamicCullingSystem->Collect_Objects(frustum);
	}

	Add_Collected_Objects_To_List(static_objs,dynamic_objs,list);
}

void PhysicsSceneClass::Collect_Collideable_Objects
(
	const AABoxClass & box,
	int colgroup,
	bool static_objs,
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);

#if defined(RENEGADE_COLLISION_FIX)
	if ( static_objs ) {
		Linux_Collect_Static_In_AABox ( box, colgroup, list );
	}
#else
	if (static_objs) {
		StaticCullingSystem->Reset_Collection();
		StaticCullingSystem->Collect_Objects(box);
	}
#endif

	if (dynamic_objs) {
		DynamicCullingSystem->Reset_Collection();
		DynamicCullingSystem->Collect_Objects(box);
	}

#if !defined(RENEGADE_COLLISION_FIX)
	Add_Collected_Collideable_Objects_To_List(colgroup,static_objs,dynamic_objs,list);
#else
	if ( dynamic_objs ) {
		PhysClass *obj = NULL;
		for (	obj = DynamicCullingSystem->Get_First_Collected_Object ();
				obj != NULL;
				obj = DynamicCullingSystem->Get_Next_Collected_Object ( obj ) )
		{
			if ( Do_Groups_Collide ( obj->Get_Collision_Group (), colgroup ) && !obj->Is_Ignore_Me () ) {
				list->Add ( obj );
			}
		}
	}
#endif
}

void PhysicsSceneClass::Collect_Collideable_Objects
(
	const OBBoxClass & box,
	int colgroup,
	bool static_objs,
	bool dynamic_objs,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);

#if defined(RENEGADE_COLLISION_FIX)
	if ( static_objs ) {
		AABoxClass query_bounds;
		query_bounds.Center = box.Center;
		box.Compute_Axis_Aligned_Extent ( &query_bounds.Extent );
		Linux_Collect_Static_In_AABox ( query_bounds, colgroup, list );
	}
#else
	if (static_objs) {
		StaticCullingSystem->Reset_Collection();
		StaticCullingSystem->Collect_Objects(box);
	}
#endif

	if (dynamic_objs) {
		DynamicCullingSystem->Reset_Collection();
		DynamicCullingSystem->Collect_Objects(box);
	}

#if !defined(RENEGADE_COLLISION_FIX)
	Add_Collected_Collideable_Objects_To_List(colgroup,static_objs,dynamic_objs,list);
#else
	if ( dynamic_objs ) {
		PhysClass *obj = NULL;
		for (	obj = DynamicCullingSystem->Get_First_Collected_Object ();
				obj != NULL;
				obj = DynamicCullingSystem->Get_Next_Collected_Object ( obj ) )
		{
			if ( Do_Groups_Collide ( obj->Get_Collision_Group (), colgroup ) && !obj->Is_Ignore_Me () ) {
				list->Add ( obj );
			}
		}
	}
#endif
}

void PhysicsSceneClass::Add_Collected_Lights_To_List
(
	bool static_lights,
	bool dynamic_lights,
	NonRefPhysListClass * list
)
{
	// link the static lights
	if (static_lights) {
		LightPhysClass * obj;
		for (	obj = StaticLightingSystem->Get_First_Collected_Object();
				obj != NULL;
				obj = StaticLightingSystem->Get_Next_Collected_Object(obj) )
		{
			list->Add(obj);
		}
	}

	// link the dynamic lights
	// TODO!!
}

void PhysicsSceneClass::Collect_Lights
(
	const Vector3 & point,
	bool static_lights,
	bool dynamic_lights,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);

	if (static_lights) {
		StaticLightingSystem->Reset_Collection();
		StaticLightingSystem->Collect_Objects(point);
	}

	// TODO: Dynamic lights!!

	Add_Collected_Lights_To_List(static_lights,dynamic_lights,list);
}


void PhysicsSceneClass::Collect_Lights
(
	const AABoxClass & bounds,
	bool static_lights,
	bool dynamic_lights,
	NonRefPhysListClass * list
)
{
	WWASSERT(list != NULL);

	if (static_lights) {
		StaticLightingSystem->Reset_Collection();
		StaticLightingSystem->Collect_Objects(bounds);
	}

	// TODO: Dynamic lights!!

	Add_Collected_Lights_To_List(static_lights,dynamic_lights,list);
}

