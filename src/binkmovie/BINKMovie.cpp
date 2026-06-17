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

#include "binkmovie.h"
#include "d3d8_lockrect.h"

#include <SDL3/SDL.h>

#include "dx8wrapper.h"
#include "formconv.h"
#include "render2d.h"
#include "Bink.h"
#include "rect.h"
#include "subtitlemanager.h"
#include "dx8caps.h"
#include "ww3d.h"
#include "menuviewport.h"
#if defined(RENEGADE_VULKAN)
#include "vk_dx8_texture.h"
#endif

class BINKMovieClass
{
	private:
		StringClass Filename;
		HBINK Bink;
		bool FrameChanged;
		unsigned TextureCount;
		unsigned long TicksPerFrame;
		unsigned long VideoMsPerFrame;
		unsigned long LastPresentMs;

		struct TextureInfoStruct {
			TextureClass* Texture;
			int TextureWidth;
			int TextureHeight;
			int TextureLocX;
			int TextureLocY;
			RectClass UV;
			RectClass Rect;
		};

		TextureInfoStruct* TextureInfos;
		unsigned char* TempBuffer;
		bool Has_Decoded_Frame;
		Render2DClass Renderer;
		SubTitleManagerClass* SubTitleManager;

		void Decode_And_Upload_Frame();

	public:
		BINKMovieClass(const char* filename,const char* subtitlename,FontCharsClass* font);
		~BINKMovieClass();

		void Update();
		void Render();
		bool Is_Complete();
		bool Should_Present_Frame();
		unsigned long Get_Ms_Per_Frame() const;
};


static BINKMovieClass* CurrentMovie;


static RectClass Compute_Bink_Display_Rect(int video_width, int video_height)
{
	int screen_width = 0;
	int screen_height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution(screen_width, screen_height, bits, windowed);

	if (video_width <= 0 || video_height <= 0 || screen_width <= 0 || screen_height <= 0) {
		return RectClass(0.0f, 0.0f, (float)screen_width, (float)screen_height);
	}

	const float video_aspect = (float)video_width / (float)video_height;
	const float screen_aspect = (float)screen_width / (float)screen_height;

	float display_width = 0.0f;
	float display_height = 0.0f;
	float x_offset = 0.0f;
	float y_offset = 0.0f;

	if (video_aspect > screen_aspect) {
		display_width = (float)screen_width;
		display_height = display_width / video_aspect;
		x_offset = 0.0f;
		y_offset = ((float)screen_height - display_height) * 0.5f;
	} else {
		display_height = (float)screen_height;
		display_width = display_height * video_aspect;
		x_offset = ((float)screen_width - display_width) * 0.5f;
		y_offset = 0.0f;
	}

	return RectClass(
		x_offset,
		y_offset,
		x_offset + display_width,
		y_offset + display_height);
}


void BINKMovie::Play(const char* filename,const char* subtitlename, FontCharsClass* font)
{
	if (CurrentMovie) {
		delete CurrentMovie;
		CurrentMovie = NULL;
	}

	CurrentMovie = new BINKMovieClass(filename,subtitlename,font);
}


void BINKMovie::Stop()
{
	if (CurrentMovie) {
		delete CurrentMovie;
		CurrentMovie = NULL;
	}
}


void BINKMovie::Update()
{
	if (CurrentMovie) {
		CurrentMovie->Update();
	}
}


void BINKMovie::Render()
{
	if (CurrentMovie) {
		CurrentMovie->Render();
	}
}


void BINKMovie::Init()
{
	BinkSoundUseDirectSound(0);
}


void BINKMovie::Shutdown()
{
	Stop();
}


bool BINKMovie::Is_Complete()
{
	if (CurrentMovie) {
		return CurrentMovie->Is_Complete();
	}

	return true;
}


bool BINKMovie::Is_Playing()
{
	return CurrentMovie != NULL && !CurrentMovie->Is_Complete();
}


unsigned long BINKMovie::Get_Ms_Per_Frame()
{
	if (CurrentMovie) {
		return CurrentMovie->Get_Ms_Per_Frame();
	}
	return 0;
}


bool BINKMovie::Should_Present_Frame()
{
	if (CurrentMovie) {
		return CurrentMovie->Should_Present_Frame();
	}
	return true;
}


// ----------------------------------------------------------------------------

BINKMovieClass::BINKMovieClass(const char* filename, const char* subtitlename, FontCharsClass* font)
	:
	Filename(filename),
	Bink(0),
	FrameChanged(true),
	TextureCount(0),
	TextureInfos(NULL),
	TempBuffer(NULL),
	Has_Decoded_Frame(false),
	TicksPerFrame(0),
	VideoMsPerFrame(67),
	LastPresentMs(0),
	SubTitleManager(NULL)
{
	Bink = BinkOpen(Filename, 0);

	if (Bink == NULL) {
		return;
	}

	if (Bink->FrameRate > 0) {
		VideoMsPerFrame =
			(1000UL * Bink->FrameRateDiv + Bink->FrameRate / 2UL) / Bink->FrameRate;
	}
	if (VideoMsPerFrame == 0) {
		VideoMsPerFrame = 67;
	}

	TempBuffer = new unsigned char[Bink->Width * Bink->Height*2];

	const D3DCAPS8& dx8caps = DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps();
	unsigned poweroftwowidth = 1;

	while (poweroftwowidth < Bink->Width) {
		poweroftwowidth <<= 1;
	}

	unsigned poweroftwoheight = 1;
	
	while (poweroftwoheight < Bink->Height) {
		poweroftwoheight <<= 1;
	}

	if (poweroftwowidth > dx8caps.MaxTextureWidth) {
		poweroftwowidth = dx8caps.MaxTextureWidth;
	}
	
	if (poweroftwoheight > dx8caps.MaxTextureHeight) {
		poweroftwoheight = dx8caps.MaxTextureHeight;
	}

	TextureCount = 0;
	unsigned max_width = poweroftwowidth;
	unsigned max_height = poweroftwoheight;
	unsigned x, y;

	for (y = 0; y < Bink->Height; y += max_height-2) {		// Two pixels are lost due to duplicated edges to prevent bilinear artifacts
		for (x = 0; x < Bink->Width; x += max_width-2) {
			++TextureCount;
		}
	}

	TextureInfos = new TextureInfoStruct[TextureCount];
	unsigned cnt = 0;
	
	for (y = 0; y < Bink->Height; y += max_height-1) {
		for (x = 0; x < Bink->Width; x += max_width-1) {
			TextureInfos[cnt].Texture = new TextureClass(
				max_width, max_height, D3DFormat_To_WW3DFormat(D3DFMT_R5G6B5),
				TextureClass::MIP_LEVELS_1, TextureClass::POOL_MANAGED, false);

			TextureInfos[cnt].TextureLocX = x;
			TextureInfos[cnt].TextureLocY = y;
			TextureInfos[cnt].TextureWidth = max_width;
			TextureInfos[cnt].UV.Right = float(max_width) / float(max_width);

			if ((TextureInfos[cnt].TextureWidth + x) > Bink->Width) {
				TextureInfos[cnt].TextureWidth = Bink->Width - x;
				TextureInfos[cnt].UV.Right = float(TextureInfos[cnt].TextureWidth - 1) / float(max_width);
			}

			TextureInfos[cnt].TextureHeight = max_height;
			TextureInfos[cnt].UV.Bottom = float(max_height) / float(max_height);

			if ((TextureInfos[cnt].TextureHeight + y) > Bink->Height) {
				TextureInfos[cnt].TextureHeight = Bink->Height - y;
				TextureInfos[cnt].UV.Bottom = float(TextureInfos[cnt].TextureHeight + 1) / float(max_height);
			}

			TextureInfos[cnt].UV.Left = 1.0f / float(max_width);
			TextureInfos[cnt].UV.Top = 1.0f / float(max_height);

			TextureInfos[cnt].Rect.Left = float(TextureInfos[cnt].TextureLocX) / float(Bink->Width);
			TextureInfos[cnt].Rect.Top = float(TextureInfos[cnt].TextureLocY) / float(Bink->Height);
			TextureInfos[cnt].Rect.Right = float(TextureInfos[cnt].TextureLocX + TextureInfos[cnt].TextureWidth) / float(Bink->Width);
			TextureInfos[cnt].Rect.Bottom = float(TextureInfos[cnt].TextureLocY + TextureInfos[cnt].TextureHeight) / float(Bink->Height);

			++cnt;
		}
	}

	Renderer.Reset();
	Renderer.Enable_Alpha(false);

	// Calculate the time per frame of video
	unsigned int rate = (Bink->FrameRate / Bink->FrameRateDiv);
	TicksPerFrame = (60 / rate);

	if (subtitlename && font) {
		SubTitleManager = SubTitleManagerClass::Create(filename, subtitlename, font);
	}
}


BINKMovieClass::~BINKMovieClass()
{
	if (Bink == NULL) {
		return;
	}

	if (Bink) {
		BinkClose(Bink);
	}

	delete[] TempBuffer;

	if (TextureInfos) {
		for (unsigned t = 0; t < TextureCount; ++t) {
			REF_PTR_RELEASE(TextureInfos[t].Texture);
		}

		delete[] TextureInfos;
	}

	if (SubTitleManager) {
		delete SubTitleManager;
	}
}


unsigned long BINKMovieClass::Get_Ms_Per_Frame() const
{
	return VideoMsPerFrame;
}


bool BINKMovieClass::Should_Present_Frame()
{
	if (!Bink || Is_Complete()) {
		return true;
	}

	const unsigned long now = (unsigned long)SDL_GetTicks();
	if (LastPresentMs == 0) {
		return true;
	}
	return (now - LastPresentMs) >= VideoMsPerFrame;
}


static unsigned char* Get_Tex_Address(unsigned char* buffer, int x, int y, int w, int h)
{
	if (x < 0) {
		x = 0;
	} else if (x >= w) {
		x = w - 1;
	}

	if (y < 0) {
		y = 0;
	} else if (y >= h) {
		y = h - 1;
	}

	return buffer + x * 2 + y * 2 * w;
}


void BINKMovieClass::Decode_And_Upload_Frame()
{
	if (!Bink || TextureCount == 0 || TextureInfos == NULL) {
		return;
	}

	const unsigned long now = (unsigned long)SDL_GetTicks();
	const bool timing_ready =
		LastPresentMs == 0 || (now - LastPresentMs) >= VideoMsPerFrame;
	const bool should_decode =
		timing_ready && (FrameChanged || !BinkWait(Bink));

	if (!should_decode) {
		return;
	}

	FrameChanged = false;
	BinkDoFrame(Bink);
	BinkCopyToBuffer(
		Bink,
		TempBuffer,
		Bink->Width * 2,
		Bink->Height,
		0,
		0,
		BINKSURFACE565 | BINKCOPYNOSCALING);

	for (unsigned t = 0; t < TextureCount; ++t) {
		unsigned char* cur_tex_ptr = Get_Tex_Address(
			TempBuffer,
			TextureInfos[t].TextureLocX,
			TextureInfos[t].TextureLocY,
			Bink->Width,
			Bink->Height);

		unsigned w = TextureInfos[t].TextureWidth;
		unsigned h = TextureInfos[t].TextureHeight;

		if (w > Bink->Width - TextureInfos[t].TextureLocX) {
			w = Bink->Width - TextureInfos[t].TextureLocX;
		}

		if (h > Bink->Height - TextureInfos[t].TextureLocY) {
			h = Bink->Height - TextureInfos[t].TextureLocY;
		}

#if defined(RENEGADE_VULKAN)
		if (DX8Wrapper::Vulkan_Device_Active()) {
			ww3d_vulkan::Upload_Procedural_Texture_Rgb565(
				TextureInfos[t].Texture,
				WW3D_FORMAT_R5G6B5,
				cur_tex_ptr,
				(int)(Bink->Width * 2),
				w,
				h);
			continue;
		}
#endif

		IDirect3DTexture8* d3d_texture = TextureInfos[t].Texture->Peek_DX8_Texture();
		if (d3d_texture) {
			D3DSURFACE_DESC d3d_surf_desc;
			RenegadeD3DLockedRect locked_rect;
			RECT rect;

			rect.left = 0;
			rect.top = 0;
			rect.right = w;
			rect.bottom = h;

			DX8_ErrorCode(d3d_texture->GetLevelDesc(0, &d3d_surf_desc));

			const HRESULT lock_hr = d3d_texture->LockRect(
				0, Renegade_AsD3DLockedRect(&locked_rect), &rect, 0);
			void *const locked_bits = Renegade_D3DLockedRect_PBits(locked_rect);

			if (FAILED(lock_hr) || locked_bits == NULL || locked_rect.Pitch <= 0) {
				DX8_ErrorCode(lock_hr);
				continue;
			}

			for (unsigned y = 0; y < h; ++y) {
				unsigned char* dest = (unsigned char*)locked_bits + y * locked_rect.Pitch;
				memcpy(dest, cur_tex_ptr, w * 2);
				cur_tex_ptr += Bink->Width * 2;
			}

			DX8_ErrorCode(d3d_texture->UnlockRect(0));
		}
	}

	if (Bink->FrameNum < Bink->Frames) {
		BinkNextFrame(Bink);
	}

	Has_Decoded_Frame = true;
	LastPresentMs = now;
}


void BINKMovieClass::Update()
{
	Decode_And_Upload_Frame();
}


void BINKMovieClass::Render()
{
	if (!Bink || TextureCount == 0 || TextureInfos == NULL) {
		return;
	}

	if (!Has_Decoded_Frame) {
		return;
	}

	// Menu pillarbox uses a 4:3 custom viewport; draw video full-screen.
	MenuViewportClass::Begin_Hud_Render();

	int screen_width = 0;
	int screen_height = 0;
	int bits = 0;
	bool windowed = false;
	WW3D::Get_Device_Resolution(screen_width, screen_height, bits, windowed);
	const RectClass screen_rect(0.0f, 0.0f, (float)screen_width, (float)screen_height);
	const RectClass display_rect = Compute_Bink_Display_Rect(Bink->Width, Bink->Height);

	for (unsigned t = 0; t < TextureCount; ++t) {
		Renderer.Reset();
		Renderer.Enable_Texturing(true);
		Renderer.Set_Texture(TextureInfos[t].Texture);
		Renderer.Set_Coordinate_Range(screen_rect);

		const RectClass &normalized_rect = TextureInfos[t].Rect;
		RectClass draw_rect;
		draw_rect.Left = display_rect.Left + normalized_rect.Left * display_rect.Width();
		draw_rect.Top = display_rect.Top + normalized_rect.Top * display_rect.Height();
		draw_rect.Right = display_rect.Left + normalized_rect.Right * display_rect.Width();
		draw_rect.Bottom = display_rect.Top + normalized_rect.Bottom * display_rect.Height();

		Renderer.Add_Quad(draw_rect, TextureInfos[t].UV, 0xffffffff);
		Renderer.Render();
	}

	if (SubTitleManager) {
		SubTitleManager->Set_Display_Rect(display_rect);
		unsigned long movieTime = (Bink->FrameNum * TicksPerFrame);
		SubTitleManager->Process(movieTime);
		SubTitleManager->Render();
	}

	MenuViewportClass::End_Hud_Render();
}


bool BINKMovieClass::Is_Complete()
{
	if (!Bink) return true;
	return (Bink->FrameNum>=Bink->Frames);
}

