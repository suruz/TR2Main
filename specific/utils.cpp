/*
 * Copyright (c) 2017 Michael Chaban. All rights reserved.
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
#include "specific/utils.h"
#include "global/vars.h"

void __cdecl Ticks() {
	LARGE_INTEGER counter;

	QueryPerformanceCounter(&counter);
	TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
}

bool __cdecl TIME_Init() {
	LARGE_INTEGER frequency;

	if( !QueryPerformanceFrequency(&frequency) )
		return false;

	TIME_Frequency = (double)frequency.QuadPart / (double)TICKS_PER_SECOND;
	Ticks();
	return true;
}

DWORD __cdecl Sync() {
	DWORD lastTicks, currentTicks;
	LARGE_INTEGER counter;

	QueryPerformanceCounter(&counter);
	lastTicks = (DWORD)TIME_Ticks;
	TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
	currentTicks = (DWORD)TIME_Ticks;

	return ( currentTicks > lastTicks ) ? currentTicks - lastTicks : 0;
}

LPVOID __cdecl UT_LoadResource(LPCTSTR lpName, LPCTSTR lpType) {
	HRSRC hRes = FindResource(GameModule, lpName, lpType);
	if( hRes == NULL )
		return NULL;

	HGLOBAL hGlb = LoadResource(GameModule, hRes);
	if( hGlb == NULL )
		return NULL;

	return LockResource(hGlb);;
}

void __cdecl UT_InitAccurateTimer() {
	LARGE_INTEGER frequency;

	TIME_Period_us = 0.0;
	TIME_Start_us = 0.0;
	TIME_Start = 0;

	if( QueryPerformanceFrequency(&frequency) ) {
		TIME_Period_us = 1.0 / (double)frequency.QuadPart; // Tick period for one microsecond
		TIME_Start_us = UT_Microseconds();
	} else {
		TIME_Start = timeGetTime();
	}
}

double __cdecl UT_Microseconds() {
	LARGE_INTEGER counter;

	if( QueryPerformanceCounter(&counter) )
		return (double)counter.QuadPart * TIME_Period_us - TIME_Start_us; // NOTE: Here was formula bug in the original code: (counter-start_us)*period
	else
		return (double)(timeGetTime() - TIME_Start) * 0.001;
}

BOOL __cdecl UT_CenterWindow(HWND hWnd) {
	int x, y;
	RECT screenArea, windowArea;

	SystemParametersInfo(SPI_GETWORKAREA, 0, &screenArea, 0);
	GetWindowRect(hWnd, &windowArea);
	x = (screenArea.left + screenArea.right) / 2 - (windowArea.right - windowArea.left) / 2;
	y = (screenArea.top + screenArea.bottom) / 2 - (windowArea.bottom - windowArea.top) / 2;

	return SetWindowPos(hWnd, 0, x, y, -1, -1, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
}

LPTSTR __cdecl UT_FindArg(LPCTSTR str) {
	LPTSTR next = strstr(CommandLinePtr, str);
	return ( next ) ? next += lstrlen(str) : NULL;
}

int __cdecl UT_MessageBox(LPCTSTR lpText, HWND hWnd) {
	return MessageBox(hWnd, lpText, MessageBoxName, MB_ICONERROR);
}

int __cdecl UT_ErrorBox(UINT uID, HWND hWnd) {
	char str[256];

	if( !LoadString(GameModule, uID, str, 256))
		return 0;

	return UT_MessageBox(str, hWnd);
}

BOOL __cdecl CD_NoteAlert(LPCTSTR lpTemplateName, HWND hWndParent) {
	return ( 0 != DialogBoxParam(GameModule, lpTemplateName, hWndParent, DialogBoxProc, 0) );
}

INT_PTR CALLBACK DialogBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch( uMsg ) {
		case WM_INITDIALOG :
			UT_CenterWindow(hwndDlg);
			break;

		case WM_COMMAND :
			switch( LOWORD(wParam) ) {
				case IDOK :
					EndDialog(hwndDlg, 1);
					return TRUE;
				case IDCANCEL :
					EndDialog(hwndDlg, 0);
					return TRUE;
			}
			break;
	}
	return FALSE;
}

void __cdecl UT_MemBlt(BYTE *dstBuf, DWORD dstX, DWORD dstY, DWORD width, DWORD height, DWORD dstPitch,
					   BYTE *srcBuf, DWORD srcX, DWORD srcY, DWORD srcPitch)
{
	BYTE *src = srcBuf + srcX + srcY * srcPitch;
	BYTE *dst = dstBuf + dstX + dstY * dstPitch;

	for( DWORD i=0; i<height; ++i ) {
		memcpy(dst, src, width);
		src += srcPitch;
		dst += dstPitch;
	}
}

/*
 * Inject function
 */
void Inject_Utils() {
	INJECT(0x00456680, Ticks);
	INJECT(0x004566C0, TIME_Init);
	INJECT(0x00456720, Sync);
	INJECT(0x00456780, UT_LoadResource);
	INJECT(0x004567C0, UT_InitAccurateTimer);
	INJECT(0x00456820, UT_Microseconds);
	INJECT(0x00456870, UT_CenterWindow);
	INJECT(0x004568E0, UT_FindArg);
	INJECT(0x00456910, UT_MessageBox);
	INJECT(0x00456930, UT_ErrorBox);
	INJECT(0x00456980, CD_NoteAlert);
	INJECT(0x004569B0, DialogBoxProc);
	INJECT(0x004574A0, UT_MemBlt);
}
