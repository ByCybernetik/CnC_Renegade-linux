#include "menuviewport.h"
#include "render2d.h"
#include "stylemgr.h"
#include "camera.h"
#include "ww3d.h"
#include "wwmath.h"
#include "vector2.h"
#include "vector3.h"

bool MenuViewportClass::Active = false;
RectClass MenuViewportClass::PhysicalScreen(0, 0, 0, 0);
RectClass MenuViewportClass::LogicalScreen(0, 0, 0, 0);
RectClass MenuViewportClass::ViewportPixels(0, 0, 0, 0);
RectClass MenuViewportClass::SavedScreenResolution(0, 0, 0, 0);
float MenuViewportClass::SavedScaleX = 1.0f;
float MenuViewportClass::SavedScaleY = 1.0f;
int MenuViewportClass::HudRenderDepth = 0;
static RectClass SavedHudRenderScreen(0, 0, 0, 0);
static bool HudResolutionDirty = false;
static unsigned int HudResolutionRevision = 0;

void MenuViewportClass::Mark_Hud_Resolution_Dirty(void)
{
	HudResolutionDirty = true;
	HudResolutionRevision++;
}

bool MenuViewportClass::Consume_Hud_Resolution_Dirty(void)
{
	if (!HudResolutionDirty) {
		return false;
	}

	HudResolutionDirty = false;
	return true;
}

unsigned int MenuViewportClass::Get_Hud_Resolution_Revision(void)
{
	return HudResolutionRevision;
}

#if defined(RENEGADE_LINUX)

void MenuViewportClass::Activate(void)
{
	int width = 0;
	int height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution(width, height, bits, windowed);

	PhysicalScreen = RectClass(0, 0, (float)width, (float)height);

	const bool widescreen = PhysicalScreen.Width() * 3.0f > PhysicalScreen.Height() * 4.0f;
	if (!widescreen) {
		if (Active) {
			Deactivate();
		}
		return;
	}

	const float virt_width = PhysicalScreen.Height() * (4.0f / 3.0f);
	const float x_offset = (PhysicalScreen.Width() - virt_width) * 0.5f;

	LogicalScreen = RectClass(0, 0, virt_width, PhysicalScreen.Height());
	ViewportPixels = RectClass(x_offset, 0, x_offset + virt_width, PhysicalScreen.Height());

	if (!Active) {
		SavedScreenResolution = Render2DClass::Get_Screen_Resolution();
		SavedScaleX = StyleMgrClass::Get_X_Scale();
		SavedScaleY = StyleMgrClass::Get_Y_Scale();
		Active = true;
	}

	Render2DClass::Set_Screen_Resolution(LogicalScreen);
	StyleMgrClass::Update_Scale_For_Resolution(LogicalScreen);
	Render2DClass::Set_Custom_Viewport(ViewportPixels);
	Mark_Hud_Resolution_Dirty();
}

void MenuViewportClass::Deactivate(void)
{
	if (!Active) {
		return;
	}

	HudRenderDepth = 0;

	Render2DClass::Set_Screen_Resolution(SavedScreenResolution);
	StyleMgrClass::Update_Scale_For_Resolution(SavedScreenResolution);
	Render2DClass::Clear_Custom_Viewport();
	Active = false;
	Mark_Hud_Resolution_Dirty();
}

void MenuViewportClass::Transform_Mouse_Pos(Vector3 &pos)
{
	if (!Active) {
		return;
	}

	pos.X -= ViewportPixels.Left;
	pos.Y -= ViewportPixels.Top;

	if (pos.X < 0.0f) {
		pos.X = 0.0f;
	}
	if (pos.Y < 0.0f) {
		pos.Y = 0.0f;
	}
	if (pos.X > LogicalScreen.Width()) {
		pos.X = LogicalScreen.Width();
	}
	if (pos.Y > LogicalScreen.Height()) {
		pos.Y = LogicalScreen.Height();
	}
}

void MenuViewportClass::Transform_Mouse_Pos(Vector2 &pos)
{
	Vector3 as_vector3(pos.X, pos.Y, 0.0f);
	Transform_Mouse_Pos(as_vector3);
	pos.X = as_vector3.X;
	pos.Y = as_vector3.Y;
}

void MenuViewportClass::Untransform_Mouse_Pos(Vector3 &pos)
{
	if (!Active) {
		return;
	}

	pos.X += ViewportPixels.Left;
	pos.Y += ViewportPixels.Top;
}

void MenuViewportClass::Apply_To_Camera(CameraClass *camera)
{
	if (!Active || camera == NULL) {
		return;
	}

	const float inv_width = 1.0f / PhysicalScreen.Width();
	const float x_min = ViewportPixels.Left * inv_width;
	const float x_max = ViewportPixels.Right * inv_width;

	camera->Set_Viewport(Vector2(x_min, 0.0f), Vector2(x_max, 1.0f));

	const float hfov = DEG_TO_RAD(45.0f);
	camera->Set_View_Plane(hfov, (3.0f / 4.0f) * hfov);
}

void MenuViewportClass::Begin_Hud_Render(void)
{
	int width = 0;
	int height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution(width, height, bits, windowed);
	PhysicalScreen = RectClass(0, 0, (float)width, (float)height);

	if (HudRenderDepth++ == 0) {
		SavedHudRenderScreen = Render2DClass::Get_Screen_Resolution();
		Render2DClass::Set_Screen_Resolution(PhysicalScreen);
		Render2DClass::Clear_Custom_Viewport();
	}
}

void MenuViewportClass::End_Hud_Render(void)
{
	if (HudRenderDepth <= 0) {
		return;
	}

	if (--HudRenderDepth == 0) {
		if (Active) {
			Render2DClass::Set_Screen_Resolution(LogicalScreen);
			Render2DClass::Set_Custom_Viewport(ViewportPixels);
		} else {
			Render2DClass::Set_Screen_Resolution(SavedHudRenderScreen);
		}
	}
}

#else /* !RENEGADE_LINUX — reserve MinGW path: no letterbox viewport */

void MenuViewportClass::Activate(void) {}
void MenuViewportClass::Deactivate(void) {}
void MenuViewportClass::Transform_Mouse_Pos(Vector3 &) {}
void MenuViewportClass::Transform_Mouse_Pos(Vector2 &) {}
void MenuViewportClass::Untransform_Mouse_Pos(Vector3 &) {}
void MenuViewportClass::Apply_To_Camera(CameraClass *) {}
void MenuViewportClass::Begin_Hud_Render(void) {}
void MenuViewportClass::End_Hud_Render(void) {}

#endif
