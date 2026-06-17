/*
 * Letterbox/pillarbox menu UI to 4:3 on widescreen displays.
 */
#ifndef __MENU_VIEWPORT_H
#define __MENU_VIEWPORT_H

#include "rect.h"

class CameraClass;
class Vector2;
class Vector3;

class MenuViewportClass
{
public:
	static void Activate(void);
	static void Deactivate(void);
	static bool Is_Active(void) { return Active; }

	static const RectClass &Get_Logical_Screen(void) { return LogicalScreen; }
	static void Transform_Mouse_Pos(Vector3 &pos);
	static void Transform_Mouse_Pos(Vector2 &pos);
	static void Untransform_Mouse_Pos(Vector3 &pos);
	static void Apply_To_Camera(CameraClass *camera);

	// Temporarily use physical widescreen 2D state while menu viewport is active.
	static void Begin_Hud_Render(void);
	static void End_Hud_Render(void);

	// Menu viewport changes the global 2D resolution; HUD must refresh afterward.
	static void Mark_Hud_Resolution_Dirty(void);
	static bool Consume_Hud_Resolution_Dirty(void);
	static unsigned int Get_Hud_Resolution_Revision(void);

private:
	static bool Active;
	static RectClass PhysicalScreen;
	static RectClass LogicalScreen;
	static RectClass ViewportPixels;
	static RectClass SavedScreenResolution;
	static float SavedScaleX;
	static float SavedScaleY;
	static int HudRenderDepth;
};

#endif
