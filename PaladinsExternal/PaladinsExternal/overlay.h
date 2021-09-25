#ifndef X2295_UTILS_OVERLAY
#define X2295_UTILS_OVERLAY 1
#ifndef FOverlay_H
#define FOverlay_H

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <memory>
#include <thread>
#include <functional>
#include <array>
#include <dwmapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <d2d1.h>
#include <dwrite.h>
#pragma comment(lib, "Dwrite")
#pragma comment(lib, "d2d1.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dwmapi.lib")
#pragma comment(lib,"d3dcompiler") 

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

class FOverlay
{
public:
	auto window_set_style()-> void;
	auto window_set_transparency()-> void;
	auto window_set_top_most()-> void;
	auto retrieve_window()->HWND;
	auto window_init()->BOOL;
	auto d2d_shutdown()-> void;
	auto init_d2d()->BOOL;
	auto begin_scene()-> void;
	auto end_scene()-> void;
	auto clear_scene()-> void;
	auto draw_text_white(int x, int y, const char* str, ...)-> void;
	auto draw_text_red(int x, int y, const char* str, ...)-> void;
	auto draw_text_green(int x, int y, const char* str, ...)-> void;
	auto clear_screen()-> void;
	auto draw_text(int x, int y, D2D1::ColorF color, const char* str, ...) -> void;
	auto draw_line(int x1, int y1, int x2, int y2, D2D1::ColorF color) -> void;
};

#endif

inline std::string string_To_UTF8(const std::string& str)
{
	int nwLen = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	ZeroMemory(pwBuf, nwLen * 2 + 2);

	::MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char* pBuf = new char[nLen + 1];
	ZeroMemory(pBuf, nLen + 1);

	::WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr(pBuf);

	delete[]pwBuf;
	delete[]pBuf;

	pwBuf = NULL;
	pBuf = NULL;

	return retStr;
}


namespace X2295
{
	static ImFont* Century_Gothic, * DefaultFont;

	using _D3DOVERLAY_USER_RENDER = std::function<VOID(FLOAT, FLOAT)>;
	class D3DOverlay
	{
	private:
		static HWND m_vWnd;
		static BOOLEAN m_vMsgLoop;
		static WNDCLASSEX m_vWndClass;
		static MSG m_vWndMsg;
		static FLOAT m_vRenderWidth;
		static FLOAT m_vRenderHeight;
		static RECT m_vOldRect;
		static _D3DOVERLAY_USER_RENDER m_vUserRender;

		static ID3D11Device* ms_vD3dDevice;
		static ID3D11DeviceContext* ms_vDeviceContext;
		static IDXGISwapChain* ms_vSwapChain;
		static ID3D11RenderTargetView* ms_vRenderTargetView;

		static auto WINAPI WndProc(HWND aHwnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam)->LRESULT;

		static auto Reset()->VOID;
		static auto CreateOverlay(LPCTSTR aWndClassName, LPCTSTR aWndName)->BOOLEAN;
		static auto CleanupOverlay()->VOID;
		static auto CreateDeviceD3D()->BOOLEAN;
		static auto CleanupDeviceD3D()->VOID;
		static auto CreateRenderTarget()->VOID;
		static auto CleanupRenderTarget()->VOID;
		static auto CreateImGuiContext()->VOID;
		static auto CleanupImGuiContext()->VOID;
	public:
		static int getWidth();
		static int getHeight();
		static auto SetUserRender(const _D3DOVERLAY_USER_RENDER& aUserRender)->VOID;
		static auto InitOverlay(LPCTSTR aWndClassName, LPCTSTR aWndName)->BOOLEAN;
		static auto UninitOverlay()->VOID;
		static auto MsgLoop()->BOOLEAN;
		static auto AttachWindow(const HWND aWnd)->BOOLEAN;
		static auto DrawCrossHair(const FLOAT aSize, ImU32 aColor)->VOID;
		static auto DrawEspBox(const ImVec2& aPos, const FLOAT aWidth, const FLOAT aHeight, ImU32 aColor, const FLOAT aLineWidth=2.f)->VOID;
		static auto DrawLine(const ImVec2& aPoint1, const ImVec2 aPoint2,ImU32 aColor, const FLOAT aLineWidth=2.f)->VOID;
		static auto DrawCircle(const ImVec2& aPoint, const FLOAT aR, ImU32 aColor, const FLOAT aLineWidth=2.f)->VOID;
		static auto DrawCircle(float x, float y, float radius, ImVec4 color, int segments = 0)->VOID;
		static auto DrawString(const ImVec2& aPos, const std::string& aString, ImU32 aColor)->VOID;
		static auto ProgressBar(float x, float y, float w, float h, int value, int v_max, ImColor barColor, bool Outlined, ImColor Outlinecolor = ImColor(0,0,0))->VOID;
		static auto DrawOutlinedText(ImFont* pFont, const std::string& text, const ImVec2& pos, float size, ImU32 color, bool center)->VOID;
		static auto DrawBox(ImColor color, float x, float y, float w, float h)->VOID;
		static auto DrawCorneredBox(float X, float Y, float W, float H, const ImU32& color, float thickness)->VOID;
		static auto RectFilled(float x0, float y0, float x1, float y1, ImColor color, float rounding, int rounding_corners_flags)->VOID;
		static auto IsInScrren(const ImVec2& aPos)->BOOLEAN;
		static auto TabButton(const char* label, int* index, int val, bool sameline)->VOID;

	};
}

#endif
