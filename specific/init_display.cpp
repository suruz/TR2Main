/*
 * Copyright (c) 2017-2020 Michael Chaban. All rights reserved.
 * Original game is written by Core Design Ltd. in 1997.
 * Lara Croft and Tomb Raider are trademarks of Square Enix Ltd.
 *
 * This file is part of TR2Main.
 *
 * TR2Main is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TR2Main is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with TR2Main.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "global/precompiled.h"
#include "game/health.h"
#include "specific/display.h"
#include "specific/init_display.h"
#include "specific/file.h"
#include "specific/hwr.h"
#include "specific/init_3d.h"
#include "specific/output.h"
#include "specific/texture.h"
#include "specific/winmain.h"
#include "specific/winvid.h"
#include "global/vars.h"

#ifdef FEATURE_BACKGROUND_IMPROVED
#include "modding/background_new.h"

extern DWORD BGND_PictureWidth;
extern DWORD BGND_PictureHeight;
#endif // FEATURE_BACKGROUND_IMPROVED

// Related to ERROR_CODE enum
static LPCTSTR ErrorStringTable[] = {
	"OK",
	"PreferredAdapterNotFound",
	"CantCreateWindow",
	"CantCreateDirectDraw",
	"CantInitRenderer",
	"CantCreateDirectInput",
	"CantCreateKeyboardDevice",
	"CantSetKBCooperativeLevel",
	"CantSetKBDataFormat",
	"CantAcquireKeyboard",
	"CantSetDSCooperativeLevel",
	"DD_SetExclusiveMode",
	"DD_ClearExclusiveMode",
	"SetDisplayMode",
	"CreateScreenBuffers",
	"GetBackBuffer",
	"CreatePalette",
	"SetPalette",
	"CreatePrimarySurface",
	"CreateBackBuffer",
	"CreateClipper",
	"SetClipperHWnd",
	"SetClipper",
	"CreateZBuffer",
	"AttachZBuffer",
	"CreateRenderBuffer",
	"CreatePictureBuffer",
	"D3D_Create",
	"CreateDevice",
	"CreateViewport",
	"AddViewport",
	"SetViewport2",
	"SetCurrentViewport",
	"ClearRenderBuffer",
	"UpdateRenderInfo",
	"GetThirdBuffer",
	"GoFullScreen",
	"GoWindowed",
	"WrongBitDepth",
	"GetPixelFormat",
	"GetDisplayMode",
};

#if defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
LPDDS CaptureBufferSurface = NULL; // used for screen capture in windowed mode
LPDDS EnvmapBufferSurface = NULL; // used as environment map for the reflection effect

static int __cdecl CreateCaptureBuffer() {
	DDSDESC dsp;

	memset(&dsp, 0, sizeof(dsp));
	dsp.dwSize = sizeof(dsp);
	dsp.dwFlags = DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS;
	dsp.dwWidth = GameVidBufWidth;
	dsp.dwHeight = GameVidBufHeight;
	dsp.ddsCaps.dwCaps = DDSCAPS_3DDEVICE|DDSCAPS_OFFSCREENPLAIN;

	if( SavedAppSettings.RenderMode == RM_Hardware )
		dsp.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;

	if FAILED(DDrawSurfaceCreate(&dsp, &CaptureBufferSurface))
		return -1;

	WinVidClearBuffer(CaptureBufferSurface, NULL, 0);
	return 0;
}
#endif // defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)

void __cdecl CreateScreenBuffers() {
#if (DIRECT3D_VERSION >= 0x700)
	DDSCAPS2 ddsCaps;
#else // (DIRECT3D_VERSION >= 0x700)
	DDSCAPS ddsCaps;
#endif // (DIRECT3D_VERSION >= 0x700)
	DDSDESC dsp;

	memset(&dsp, 0, sizeof(dsp));
	dsp.dwSize = sizeof(dsp);
	dsp.dwFlags = DDSD_BACKBUFFERCOUNT|DDSD_CAPS;
	dsp.dwBackBufferCount = (SavedAppSettings.TripleBuffering) ? 2 : 1;
	dsp.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
	if( SavedAppSettings.RenderMode == RM_Hardware )
		dsp.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE;

	if FAILED(DDrawSurfaceCreate(&dsp, &PrimaryBufferSurface))
		throw ERR_CreateScreenBuffers;

	WinVidClearBuffer(PrimaryBufferSurface, NULL, 0);

	memset(&ddsCaps, 0, sizeof(ddsCaps));
	ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
	if FAILED(PrimaryBufferSurface->GetAttachedSurface(&ddsCaps, &BackBufferSurface))
		throw ERR_GetBackBuffer;

	WinVidClearBuffer(BackBufferSurface, NULL, 0);

	if( SavedAppSettings.TripleBuffering ) {
		memset(&ddsCaps, 0, sizeof(ddsCaps));
		ddsCaps.dwCaps = DDSCAPS_FLIP;
		if FAILED(BackBufferSurface->GetAttachedSurface(&ddsCaps, &ThirdBufferSurface))
			throw ERR_GetThirdBuffer;

		WinVidClearBuffer(ThirdBufferSurface, NULL, 0);
	}
}

void __cdecl CreatePrimarySurface() {
	DDSDESC dsp;

	if( ( GameVid_IsVga && SavedAppSettings.RenderMode == RM_Hardware) ||
		(!GameVid_IsVga && SavedAppSettings.RenderMode == RM_Software) )
		throw ERR_WrongBitDepth;

	memset(&dsp, 0, sizeof(dsp));
	dsp.dwSize = sizeof(dsp);
	dsp.dwFlags = DDSD_CAPS;
	dsp.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	if FAILED(DDrawSurfaceCreate(&dsp, &PrimaryBufferSurface))
		throw ERR_CreatePrimarySurface;
}

void __cdecl CreateBackBuffer() {
	DDSDESC dsp;

	memset(&dsp, 0, sizeof(dsp));
	dsp.dwSize = sizeof(dsp);
	dsp.dwFlags = DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS;
	dsp.dwWidth = GameVidBufWidth;
	dsp.dwHeight = GameVidBufHeight;
	dsp.ddsCaps.dwCaps = DDSCAPS_3DDEVICE|DDSCAPS_OFFSCREENPLAIN;

	if( SavedAppSettings.RenderMode == RM_Hardware )
		dsp.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;

	if FAILED(DDrawSurfaceCreate(&dsp, &BackBufferSurface))
		throw ERR_CreateBackBuffer;

	WinVidClearBuffer(BackBufferSurface, NULL, 0);
}

void __cdecl CreateClipper() {

	if FAILED(DDraw->CreateClipper(0, &DDrawClipper, NULL))
		throw ERR_CreateClipper;

	if FAILED(DDrawClipper->SetHWnd(0, HGameWindow))
		throw ERR_SetClipperHWnd;

	if FAILED(PrimaryBufferSurface->SetClipper(DDrawClipper))
		throw ERR_SetClipper;
}

void __cdecl CreateWindowPalette() {
	int i;
	DWORD dwFlags = DDPCAPS_8BIT;

	memset(WinVidPalette, 0, sizeof(WinVidPalette));

	if( GameVid_IsWindowedVga ) {
		for( i=0; i<10; ++i ) {
			WinVidPalette[i].peFlags = PC_EXPLICIT;
			WinVidPalette[i].peRed   = i;
		}
		for( i=10; i<246; ++i ) {
			WinVidPalette[i].peFlags = PC_NOCOLLAPSE | PC_RESERVED;
		}
		for( i=246; i<256; ++i ) {
			WinVidPalette[i].peFlags = PC_EXPLICIT;
			WinVidPalette[i].peRed   = i;
		}
	} else {
		for( i=0; i<256; ++i ) {
			WinVidPalette[i].peFlags = PC_RESERVED;
		}
		dwFlags |= DDPCAPS_ALLOW256;
	}

	if FAILED(DDraw->CreatePalette(dwFlags, WinVidPalette, &DDrawPalette, NULL))
		throw ERR_CreatePalette;

	if FAILED(PrimaryBufferSurface->SetPalette(DDrawPalette))
		throw ERR_SetPalette;
}

#if (DIRECT3D_VERSION >= 0x700)
static HRESULT WINAPI EnumZBufferCallback(DDPIXELFORMAT* pddpf, VOID* pddpfDesired) {
	if( pddpf->dwFlags == DDPF_ZBUFFER ) {
		memcpy(pddpfDesired, pddpf, sizeof(DDPIXELFORMAT));
		return D3DENUMRET_CANCEL;
	}
	return D3DENUMRET_OK;
}
#endif // (DIRECT3D_VERSION >= 0x700)

void __cdecl CreateZBuffer() {
	DDSDESC dsp;

	if( (CurrentDisplayAdapter.D3DHWDeviceDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_ZBUFFERLESSHSR) != 0 )
		return;

	memset(&dsp, 0, sizeof(dsp));
	dsp.dwSize = sizeof(dsp);
	dsp.dwWidth = GameVidBufWidth;
	dsp.dwHeight = GameVidBufHeight;
#if (DIRECT3D_VERSION >= 0x700)
	dsp.dwFlags = DDSD_PIXELFORMAT|DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS;
	if( D3D == NULL && !D3DCreate() )
		throw ERR_D3D_Create;
	D3D->EnumZBufferFormats(IID_IDirect3DHALDevice, EnumZBufferCallback, &dsp.ddpfPixelFormat);
#else // (DIRECT3D_VERSION >= 0x700)
	dsp.dwFlags = DDSD_ZBUFFERBITDEPTH|DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS;
	dsp.dwZBufferBitDepth = GetZBufferDepth();
#endif // (DIRECT3D_VERSION >= 0x700)
	dsp.ddsCaps.dwCaps = DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;

	if FAILED(DDrawSurfaceCreate(&dsp, &ZBufferSurface))
		throw ERR_CreateZBuffer;

	if FAILED(BackBufferSurface->AddAttachedSurface(ZBufferSurface))
		throw ERR_AttachZBuffer;
}

DWORD __cdecl GetZBufferDepth() {
	DWORD bitDepthMask = CurrentDisplayAdapter.D3DHWDeviceDesc.dwDeviceZBufferBitDepth;
	if( (bitDepthMask & DDBD_16) != 0 ) return 16;
	if( (bitDepthMask & DDBD_24) != 0 ) return 24;
	if( (bitDepthMask & DDBD_32) != 0 ) return 32;
	return 8;
}

void __cdecl CreateRenderBuffer() {
	DDSDESC dsp;

	memset(&dsp, 0, sizeof(dsp));
	dsp.dwSize = sizeof(dsp);
	dsp.dwFlags = DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS;
	dsp.dwWidth = GameVidBufWidth;
	dsp.dwHeight = GameVidBufHeight;
	dsp.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY|DDSCAPS_OFFSCREENPLAIN;

	if FAILED(DDrawSurfaceCreate(&dsp, &RenderBufferSurface))
		throw ERR_CreateRenderBuffer;

	if( !WinVidClearBuffer(RenderBufferSurface, NULL, 0) )
		throw ERR_ClearRenderBuffer;
}

void __cdecl CreatePictureBuffer() {
	DDSDESC dsp;

	memset(&dsp, 0, sizeof(dsp));
	dsp.dwSize = sizeof(dsp);
	dsp.dwFlags = DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS;
	dsp.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY|DDSCAPS_OFFSCREENPLAIN;
#ifdef FEATURE_BACKGROUND_IMPROVED
	dsp.dwWidth = BGND_PictureWidth;
	dsp.dwHeight = BGND_PictureHeight;
#else // !FEATURE_BACKGROUND_IMPROVED
	dsp.dwWidth = 640;
	dsp.dwHeight = 480;
#endif // FEATURE_BACKGROUND_IMPROVED

	if FAILED(DDrawSurfaceCreate(&dsp, &PictureBufferSurface))
		throw ERR_CreatePictureBuffer;
}

void __cdecl ClearBuffers(DWORD flags, DWORD fillColor) {
	DWORD d3dClearFlags = 0;
	D3DRECT d3dRect;
	RECT winRect;

	if( (flags & CLRB_PhdWinSize) != 0 ) {
		winRect.left = PhdWinMinX;
		winRect.top  = PhdWinMinY;
		winRect.right  = PhdWinMinX + PhdWinWidth;
		winRect.bottom = PhdWinMinY + PhdWinHeight;
	} else {
		winRect.left = 0;
		winRect.top  = 0;
		winRect.right  = GameVidWidth;
		winRect.bottom = GameVidHeight;
	}

	if( SavedAppSettings.RenderMode == RM_Hardware ) {
	// Hardware Renderer Checks
		if( (flags & CLRB_BackBuffer) != 0 )
			d3dClearFlags |= D3DCLEAR_TARGET;

		if( (flags & CLRB_ZBuffer) != 0 )
			d3dClearFlags |= D3DCLEAR_ZBUFFER;

		if( d3dClearFlags ) {
			d3dRect.x1 = winRect.left;
			d3dRect.y1 = winRect.top;
			d3dRect.x2 = winRect.right;
			d3dRect.y2 = winRect.bottom;
#if (DIRECT3D_VERSION >= 0x700)
			D3DDev->Clear(1, &d3dRect, d3dClearFlags, fillColor, 1.0, 0);
#else // (DIRECT3D_VERSION >= 0x700)
			D3DView->Clear(1, &d3dRect, d3dClearFlags);
#endif // (DIRECT3D_VERSION >= 0x700)
		}
	} else {
	// Software Renderer Checks
		if( (flags & CLRB_BackBuffer) != 0 )
			WinVidClearBuffer(BackBufferSurface, &winRect, fillColor);
	}

	// Common checks
	if( (flags & CLRB_PrimaryBuffer) != 0 ) {
		WinVidClearBuffer(PrimaryBufferSurface, &winRect, fillColor);
#if defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
		if( CaptureBufferSurface != NULL ) {
			WinVidClearBuffer(CaptureBufferSurface, &winRect, fillColor);
		}
#endif // defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
	}

	if( (flags & CLRB_ThirdBuffer) != 0 )
		WinVidClearBuffer(ThirdBufferSurface, &winRect, fillColor);

	if( (flags & CLRB_RenderBuffer) != 0 )
		WinVidClearBuffer(RenderBufferSurface, &winRect, fillColor);

	if( (flags & CLRB_PictureBuffer) != 0 ) {
		winRect.left = 0;
		winRect.top  = 0;
#ifdef FEATURE_BACKGROUND_IMPROVED
		winRect.right = BGND_PictureWidth;
		winRect.bottom = BGND_PictureHeight;
#else // !FEATURE_BACKGROUND_IMPROVED
		winRect.right = 640;
		winRect.bottom = 480;
#endif // FEATURE_BACKGROUND_IMPROVED
		WinVidClearBuffer(PictureBufferSurface, &winRect, fillColor);
	}

	if( (flags & CLRB_WindowedPrimaryBuffer) != 0 ) {
		winRect.left = GameWindowPositionX;
		winRect.top  = GameWindowPositionY;
		winRect.right  = GameWindowPositionX + GameWindowWidth;
		winRect.bottom = GameWindowPositionY + GameWindowHeight;
		WinVidClearBuffer(PrimaryBufferSurface, &winRect, fillColor);
	}
}

void __cdecl RestoreLostBuffers() {
	if( !PrimaryBufferSurface ) {
		S_ExitSystem("Oops... no front buffer");
		return; // the app is terminated here
	}
	if FAILED(DDrawSurfaceRestoreLost(PrimaryBufferSurface, NULL, SavedAppSettings.FullScreen)) {
		goto REBUILD;
	}
	if( SavedAppSettings.FullScreen || SavedAppSettings.RenderMode == RM_Hardware ) {
		if FAILED(DDrawSurfaceRestoreLost(BackBufferSurface, PrimaryBufferSurface, true))
			goto REBUILD;
	}
	if( SavedAppSettings.TripleBuffering ) {
		if FAILED(DDrawSurfaceRestoreLost(ThirdBufferSurface, PrimaryBufferSurface, true))
			goto REBUILD;
	}
	if( SavedAppSettings.RenderMode == RM_Software ) {
		if FAILED(DDrawSurfaceRestoreLost(RenderBufferSurface, NULL, false))
			goto REBUILD;
	}
	if( ZBufferSurface ) {
		if FAILED(DDrawSurfaceRestoreLost(ZBufferSurface, NULL, false))
			goto REBUILD;
	}
	if( PictureBufferSurface ) {
		if FAILED(DDrawSurfaceRestoreLost(PictureBufferSurface, NULL, false))
			goto REBUILD;
	}
#if defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
	if( CaptureBufferSurface ) {
		if FAILED(DDrawSurfaceRestoreLost(CaptureBufferSurface, PrimaryBufferSurface, true))
			goto REBUILD;
	}
	if( EnvmapBufferSurface ) {
		if FAILED(DDrawSurfaceRestoreLost(EnvmapBufferSurface, NULL, false))
			goto REBUILD;
	}
#endif // defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
	return;

REBUILD:
	if( !IsGameToExit ) {
		ApplySettings(&SavedAppSettings);
		if( SavedAppSettings.RenderMode == RM_Hardware ) {
			HWR_GetPageHandles();
		}
	}
}

void __cdecl UpdateFrame(bool needRunMessageLoop, LPRECT rect) {
	RECT dstRect;
	LPRECT pSrcRect = rect ? rect : &GameVidRect;

	RestoreLostBuffers();
	if( SavedAppSettings.FullScreen ) {
		if( SavedAppSettings.RenderMode == RM_Software ) {
			BackBufferSurface->Blt(pSrcRect, RenderBufferSurface, pSrcRect, DDBLT_WAIT, NULL);
		}
		PrimaryBufferSurface->Flip(NULL, DDFLIP_WAIT);
	} else {
		dstRect.left = GameWindowPositionX + pSrcRect->left;
		dstRect.top = GameWindowPositionY + pSrcRect->top;
		dstRect.right = GameWindowPositionX + pSrcRect->right;
		dstRect.bottom = GameWindowPositionY + pSrcRect->bottom;

		LPDDS surface = ( SavedAppSettings.RenderMode == RM_Software ) ? RenderBufferSurface : BackBufferSurface;
		PrimaryBufferSurface->Blt(&dstRect, surface, pSrcRect, DDBLT_WAIT, NULL);
#if defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
		if( CaptureBufferSurface != NULL ) {
			CaptureBufferSurface->Blt(pSrcRect, BackBufferSurface, pSrcRect, DDBLT_WAIT, NULL);
		}
#endif // defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
	}

	if( needRunMessageLoop ) {
		WinVidSpinMessageLoop(false);
	}
}

void __cdecl WaitPrimaryBufferFlip() {
	if( SavedAppSettings.FlipBroken && SavedAppSettings.FullScreen ) {
		while( PrimaryBufferSurface->GetFlipStatus(DDGFS_ISFLIPDONE) == DDERR_WASSTILLDRAWING )
			/* just wait */;
	}
}

bool __cdecl RenderInit() {
	return true;
}

void __cdecl RenderStart(bool isReset) {
	signed int minWidth;
	signed int minHeight;
	bool is16bitTextures;
	DISPLAY_MODE dispMode;
	DDPIXELFORMAT pixelFormat;

	if( isReset )
		NeedToReloadTextures = false;

	if( SavedAppSettings.FullScreen ) {
		// FullScreen mode

		if( SavedAppSettings.VideoMode == NULL )
			throw ERR_GoFullScreen;

		dispMode.width  = SavedAppSettings.VideoMode->body.width;
		dispMode.height = SavedAppSettings.VideoMode->body.height;
		dispMode.bpp = SavedAppSettings.VideoMode->body.bpp;
		dispMode.vga = SavedAppSettings.VideoMode->body.vga;

		if( !WinVidGoFullScreen(&dispMode) )
			throw ERR_GoFullScreen;

		CreateScreenBuffers();

		GameVidWidth  = dispMode.width;
		GameVidHeight = dispMode.height;
		GameVidBufWidth  = dispMode.width;
		GameVidBufHeight = dispMode.height;
		GameVidBPP = dispMode.bpp;
		GameVid_IsVga = ( dispMode.vga != 0 );
		GameVid_IsWindowedVga = false;
		GameVid_IsFullscreenVga = ( dispMode.vga == VGA_Standard );
	} else {
		// Windowed mode

		minWidth = 320;
		minHeight = CalculateWindowHeight(320, 200);
		if( minHeight < 200 ) {
			minWidth = CalculateWindowWidth(320, 200);
			minHeight = 200;
		}

		WinVidSetMinWindowSize(minWidth, minHeight);
		if( !WinVidGoWindowed(SavedAppSettings.WindowWidth, SavedAppSettings.WindowHeight, &dispMode) )
			throw ERR_GoWindowed;

		GameVidWidth  = dispMode.width;
		GameVidHeight = dispMode.height;
		GameVidBufWidth  = (dispMode.width  + 0x1F) & ~0x1F;
		GameVidBufHeight = (dispMode.height + 0x1F) & ~0x1F;
		GameVidBPP = dispMode.bpp;
		GameVid_IsVga = ( dispMode.vga != 0 );
		GameVid_IsWindowedVga = ( dispMode.vga != 0 );
		GameVid_IsFullscreenVga = false;

		CreatePrimarySurface();
		if( SavedAppSettings.RenderMode == RM_Hardware ) {
			CreateBackBuffer();
#if defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
			CreateCaptureBuffer();
#endif // defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
		}

		CreateClipper();
	}

	memset(&pixelFormat, 0, sizeof(DDPIXELFORMAT));
	pixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	if FAILED(PrimaryBufferSurface->GetPixelFormat(&pixelFormat))
		throw ERR_GetPixelFormat;

	WinVidGetColorBitMasks(&ColorBitMasks, &pixelFormat);
	if( GameVid_IsVga )
		CreateWindowPalette();

	if( SavedAppSettings.RenderMode == RM_Hardware ) {
		if( SavedAppSettings.ZBuffer ) {
			CreateZBuffer();
		}
		D3DDeviceCreate(BackBufferSurface);
		EnumerateTextureFormats();
	} else {
		CreateRenderBuffer();
		if( PictureBufferSurface == NULL ) {
			CreatePictureBuffer();
		}
	}

	if( NeedToReloadTextures ) {
		is16bitTextures = ( TextureFormat.bpp >= 16 );
		if( IsWindowedVGA != GameVid_IsWindowedVga || Is16bitTextures != is16bitTextures ) {
			S_ReloadLevelGraphics(IsWindowedVGA != GameVid_IsWindowedVga, Is16bitTextures != is16bitTextures );
			IsWindowedVGA = GameVid_IsWindowedVga;
			Is16bitTextures = is16bitTextures;
		} else if( SavedAppSettings.RenderMode == RM_Hardware ) {
			ReloadTextures(true);
			HWR_GetPageHandles();
		}
	} else {
		IsWindowedVGA = GameVid_IsWindowedVga;
		Is16bitTextures = ( TextureFormat.bpp >= 16 );
	}

	GameVidBufRect.left = 0;
	GameVidBufRect.top  = 0;
	GameVidBufRect.right  = GameVidBufWidth;
	GameVidBufRect.bottom = GameVidBufHeight;

	GameVidRect.left = 0;
	GameVidRect.top  = 0;
	GameVidRect.right  = GameVidWidth;
	GameVidRect.bottom = GameVidHeight;

	DumpWidth  = GameVidWidth;
	DumpHeight = GameVidHeight;

	setup_screen_size();
	NeedToReloadTextures = true;
}

void __cdecl RenderFinish(bool needToClearTextures) {
	if( SavedAppSettings.RenderMode == RM_Hardware ) {
		// Hardware Renderer
		if( needToClearTextures ) {
			S_DontDisplayPicture(); // NOTE: this line was absent in the original game
			HWR_FreeTexturePages();
			CleanupTextures();
		}
		Direct3DRelease();

		if( ZBufferSurface != NULL ) {
			ZBufferSurface->Release();
			ZBufferSurface = NULL;
		}
	} else {
		// Software Renderer
		if( needToClearTextures && PictureBufferSurface != NULL ) {
			PictureBufferSurface->Release();
			PictureBufferSurface = NULL;
		}

		if( RenderBufferSurface != NULL ) {
			RenderBufferSurface->Release();
			RenderBufferSurface = NULL;
		}
	}

	if( DDrawPalette != NULL ) {
		DDrawPalette->Release();
		DDrawPalette = NULL;
	}

	if( DDrawClipper != NULL ) {
		DDrawClipper->Release();
		DDrawClipper = NULL;
	}

	if( ThirdBufferSurface != NULL ) {
		ThirdBufferSurface->Release();
		ThirdBufferSurface = NULL;
	}

	if( BackBufferSurface != NULL ) {
		BackBufferSurface->Release();
		BackBufferSurface = NULL;
	}

#if defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)
	if( CaptureBufferSurface != NULL ) {
		CaptureBufferSurface->Release();
		CaptureBufferSurface = NULL;
	}
	if( EnvmapBufferSurface != NULL ) {
		EnvmapBufferSurface->Release();
		EnvmapBufferSurface = NULL;
	}
#endif // defined(FEATURE_SCREENSHOT_IMPROVED) || defined(FEATURE_BACKGROUND_IMPROVED) || defined(FEATURE_VIDEOFX_IMPROVED)

	if( PrimaryBufferSurface != NULL ) {
		PrimaryBufferSurface->Release();
		PrimaryBufferSurface = NULL;
	}

	if( needToClearTextures )
		NeedToReloadTextures = false;
}

bool __cdecl ApplySettings(APP_SETTINGS *newSettings) {
	char modeString[64] = {0};
	APP_SETTINGS oldSettings = SavedAppSettings;

	RenderFinish(false);

	if( newSettings != &SavedAppSettings )
		SavedAppSettings = *newSettings;

	try {
		RenderStart(false);
	}
	catch(...) {
		RenderFinish(false);
		SavedAppSettings = oldSettings;
		try {
			RenderStart(false);
		}
		catch(...) {
			DISPLAY_MODE_LIST *modeList;
			DISPLAY_MODE_NODE *mode;
			DISPLAY_MODE targetMode;

			RenderFinish(false);

			SavedAppSettings.PreferredDisplayAdapter = PrimaryDisplayAdapter;
			SavedAppSettings.RenderMode = RM_Software;
			SavedAppSettings.FullScreen = true;
			SavedAppSettings.TripleBuffering = false;

			targetMode.width = 640;
			targetMode.height = 480;
			targetMode.bpp = 0;
			targetMode.vga = VGA_NoVga;

			modeList = &PrimaryDisplayAdapter->body.swDispModeList;

			for( mode = modeList->head; mode; mode = mode->next ) {
				if( !CompareVideoModes(&mode->body, &targetMode) )
					break;
			}
			SavedAppSettings.VideoMode = mode ? mode : modeList->tail;
			try {
				RenderStart(false);
			}
			catch(...) {
				S_ExitSystem("Can't reinitialise renderer");
				return false; // the app is terminated here
			}
		}
	}
	S_InitialiseScreen(GFL_NOLEVEL);

	if( SavedAppSettings.RenderMode != oldSettings.RenderMode ) {
		S_ReloadLevelGraphics(1, 1);
	}
	else if( SavedAppSettings.RenderMode == RM_Software &&
		SavedAppSettings.FullScreen != oldSettings.FullScreen )
	{
		S_ReloadLevelGraphics(1, 0);
	}

	if( SavedAppSettings.FullScreen )
		sprintf(modeString, "%dx%dx%d", GameVidWidth, GameVidHeight, GameVidBPP);
	else
		sprintf(modeString, "%dx%d", GameVidWidth, GameVidHeight);

	DisplayModeInfo(modeString);
	return true;
}

void __cdecl FmvBackToGame() {
	try {
		RenderStart(true);
	}
	catch(...) {
		DISPLAY_MODE_LIST *modeList;
		DISPLAY_MODE_NODE *mode;
		DISPLAY_MODE targetMode;

		RenderFinish(false);

		SavedAppSettings.PreferredDisplayAdapter = PrimaryDisplayAdapter;
		SavedAppSettings.RenderMode = RM_Software;
		SavedAppSettings.FullScreen = true;
		SavedAppSettings.TripleBuffering = false;

		targetMode.width = 640;
		targetMode.height = 480;
		targetMode.bpp = 0;
		targetMode.vga = VGA_NoVga;

		modeList = &PrimaryDisplayAdapter->body.swDispModeList;

		for( mode = modeList->head; mode; mode = mode->next ) {
			if( !CompareVideoModes(&mode->body, &targetMode) )
				break;
		}
		SavedAppSettings.VideoMode = mode ? mode : modeList->tail;
		try {
			RenderStart(false);
		}
		catch(...) {
			S_ExitSystem("Can't reinitialise renderer");
			return; // the app is terminated here
		}
	}
}

void __cdecl GameApplySettings(APP_SETTINGS *newSettings) {
	bool needInitRenderState = false;
	bool needRebuildBuffers = false;
	bool needAdjustTexel = false;
	DISPLAY_MODE dispMode;

	if( newSettings->PreferredDisplayAdapter != SavedAppSettings.PreferredDisplayAdapter||
		newSettings->PreferredSoundAdapter != SavedAppSettings.PreferredSoundAdapter ||
		newSettings->PreferredJoystick != SavedAppSettings.PreferredJoystick )
	{
		return;
	}

	if( newSettings->PerspectiveCorrect != SavedAppSettings.PerspectiveCorrect ||
		newSettings->Dither != SavedAppSettings.Dither ||
		newSettings->BilinearFiltering != SavedAppSettings.BilinearFiltering )
	{
		needInitRenderState = true;
	}

	if( newSettings->BilinearFiltering != SavedAppSettings.BilinearFiltering ) {
		needAdjustTexel = true;
	}

	if( newSettings->RenderMode != SavedAppSettings.RenderMode ||
		newSettings->VideoMode  != SavedAppSettings.VideoMode ||
		newSettings->FullScreen != SavedAppSettings.FullScreen ||
		newSettings->ZBuffer    != SavedAppSettings.ZBuffer ||
		newSettings->TripleBuffering != SavedAppSettings.TripleBuffering )
	{
		ApplySettings(newSettings);
		S_AdjustTexelCoordinates();
		return;
	}

	if( !newSettings->FullScreen ) {
		if( newSettings->WindowWidth != SavedAppSettings.WindowWidth || newSettings->WindowHeight != SavedAppSettings.WindowHeight ) {
			if( !WinVidGoWindowed(newSettings->WindowWidth, newSettings->WindowHeight, &dispMode) ) {
				return;
			}
			newSettings->WindowWidth  = dispMode.width;
			newSettings->WindowHeight = dispMode.height;
			if( dispMode.width != SavedAppSettings.WindowWidth || dispMode.height != SavedAppSettings.WindowHeight ) {
				if( GameVidBufWidth < dispMode.width || GameVidBufWidth - dispMode.width > 0x40 ||
					GameVidBufHeight < dispMode.height || GameVidBufHeight - dispMode.height > 0x40 )
				{
					needRebuildBuffers = true;
				} else {
					SavedAppSettings.WindowWidth  = dispMode.width;
					SavedAppSettings.WindowHeight = dispMode.height;
					GameVidWidth  = dispMode.width;
					GameVidHeight = dispMode.height;
					GameVidRect.right  = dispMode.width;
					GameVidRect.bottom = dispMode.height;
					if( SavedAppSettings.RenderMode == RM_Hardware ) {
						D3DSetViewport();
					}
					setup_screen_size();
					WinVidNeedToResetBuffers = false;
				}
			}
		}
	}

	if( needInitRenderState ) {
		SavedAppSettings.PerspectiveCorrect = newSettings->PerspectiveCorrect;
		SavedAppSettings.Dither = newSettings->Dither;
		SavedAppSettings.BilinearFiltering = newSettings->BilinearFiltering;

		if( SavedAppSettings.RenderMode == RM_Hardware ) {
			HWR_InitState();
		}
	}

	if( needRebuildBuffers ) {
		ClearBuffers(CLRB_WindowedPrimaryBuffer, 0);
		ApplySettings(newSettings);
	}

	if( needAdjustTexel ) {
		S_AdjustTexelCoordinates();
	}
}

void __cdecl UpdateGameResolution() {
	APP_SETTINGS newSettings = SavedAppSettings;
	char modeString[64] = {0};

	newSettings.WindowWidth  = GameWindowWidth;
	newSettings.WindowHeight = GameWindowHeight;
	GameApplySettings(&newSettings);

	sprintf(modeString, "%dx%d", GameVidWidth, GameVidHeight);
	DisplayModeInfo(modeString);
}

LPCTSTR __cdecl DecodeErrorMessage(DWORD errorCode) {
	return ErrorStringTable[errorCode];
}

/*
 * Inject function
 */
void Inject_InitDisplay() {
	INJECT(0x004484E0, CreateScreenBuffers);
	INJECT(0x00448620, CreatePrimarySurface);
	INJECT(0x004486C0, CreateBackBuffer);
	INJECT(0x00448760, CreateClipper);
	INJECT(0x00448800, CreateWindowPalette);
	INJECT(0x004488E0, CreateZBuffer);
	INJECT(0x004489A0, GetZBufferDepth);
	INJECT(0x004489D0, CreateRenderBuffer);
	INJECT(0x00448A80, CreatePictureBuffer);
	INJECT(0x00448AF0, ClearBuffers);
	INJECT(0x00448CA0, RestoreLostBuffers);
	INJECT(0x00448DE0, UpdateFrame);
	INJECT(0x00448EB0, WaitPrimaryBufferFlip);
	INJECT(0x00448EF0, RenderInit);
	INJECT(0x00448F00, RenderStart);
	INJECT(0x004492B0, RenderFinish);
	INJECT(0x004493A0, ApplySettings);
	INJECT(0x004495B0, FmvBackToGame);
	INJECT(0x004496C0, GameApplySettings);
	INJECT(0x00449900, UpdateGameResolution);
	INJECT(0x00449970, DecodeErrorMessage);
}
