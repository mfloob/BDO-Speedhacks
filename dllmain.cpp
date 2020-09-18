#define WINVER 0x0500
#include <Windows.h>
#include <intrin.h>
#include <vector>
#include <d3d11.h>
#include <D3Dcompiler.h>
#include <stdlib.h>
#include "pmemory.h"
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "detours.h"
#if defined _M_X64
#pragma comment(lib, "detours.X64/detours.lib")
#elif defined _M_IX86
#pragma comment(lib, "detours.X86/detours.lib")
#endif

#pragma warning( disable : 4244 )


typedef HRESULT(__stdcall* D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall* D3D11ResizeBuffersHook) (IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
typedef void(__stdcall* D3D11PSSetShaderResourcesHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
typedef void(__stdcall* D3D11DrawHook) (ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation);

#define DEPTH_BIAS_D32_FLOAT(d) (d/(1/pow(2,23)))

D3D11PresentHook phookD3D11Present = NULL;
D3D11ResizeBuffersHook phookD3D11ResizeBuffers = NULL;
D3D11PSSetShaderResourcesHook phookD3D11PSSetShaderResources = NULL;
D3D11DrawHook phookD3D11Draw = NULL;

IDXGISwapChain* SwapChain;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

#include "main.h"
#include <iostream>
#include <tchar.h>
#include "offsets.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool imGuiInitializing = false;

LocalPlayer localPlayer;
LocalMount localMount;
CharacterControl characterControl;
CharacterScene characterScene;


void GetAddresses()
{
	try
	{
		//ConsoleSetup();
		auto playerOffset = FindPattern(GetModuleHandleW(0), (unsigned char*)"\x48\x8B\x1D\x00\x00\x00\x00\x48\x85\xDB\x0F\x84\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x80\x78\x20\x00", "xxx????xxxxx????x????xxxx");																			   
		auto player = Read<UINT32>(playerOffset + 0x3);
		localPlayer.base = player + playerOffset + 0x7;
		auto mountOffset = FindPattern(GetModuleHandleW(0), (unsigned char*)"\x4C\x8B\x25\x00\x00\x00\x00\x4D\x85\xE4\x0F\x84\x00\x00\x00\x00\x41\x8B\x8C\x24\x00\x00\x00\x00", "xxx????xxxxx????xxxx????"); //48 8B 1D ? ? ? ? 48 85 DB 0F 84 ? ? ? ? 8B 8B
		auto mount = Read<UINT32>(mountOffset + 0x3);
		localMount.base = mount + mountOffset + 0x7;

		PlayerNOP = FindPattern(GetModuleHandleW(0), (unsigned char*)"\x42\x89\xB4\xBF\x00\x00\x00\x00", "xxxx????"); //89 B4 87 ? ? ? ? 44 89 A4 87 ? ? ? ?
		MountNOP = FindPattern(GetModuleHandleW(0), (unsigned char*)"\x41\x89\x84\x8B\x00\x00\x00\x00", "xxxx????");
	}

	catch (exception e) {}
	if (!localPlayer.base || !localMount.base || !PlayerNOP)
	{
		ConsoleSetup();
		std::cout << "Error finding addresses. " << localPlayer.base << " " << localMount.base << std::endl;
	}
}

void RenderMenu()
{
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();

	if (greetings)
	{
		ImVec4 Bgcol = ImColor(0.06f, 0.05f, 0.07f, 1.00f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bgcol);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));

		ImGui::Begin("", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		ImGui::Text("Blanched loaded, press INSERT for menu");
		ImGui::End();

		static DWORD lastTime = timeGetTime();
		DWORD timePassed = timeGetTime() - lastTime;
		if (timePassed > 6000)
		{
			greetings = false;
			lastTime = timeGetTime();
		}
	}

	if (ShowMenu)
	{
		ImGui::SetNextWindowSize(ImVec2(575.0f, 325.0f));

		ImVec4 Bgcol = ImColor(0.06f, 0.05f, 0.07f, 1.00f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bgcol);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));

		ImGui::Begin("Menu", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

		ImGui::Text("F11 - Enable/Disable Attack/Mount Speed");
		ImGui::Text("");
		ImGui::Checkbox("F9 - Enable/Disable Player Speed", &speedStart);
		ImGui::SliderInt("Movement Speed", &moveSpeedVal, 90000, 10000000);
		ImGui::SliderInt("Attack Speed", &attackSpeedVal, 90000, 10000000);
		ImGui::SliderInt("Cast Speed", &castSpeedVal, 90000, 10000000);
		ImGui::SliderFloat("Animation Speed", &animationSpeedVal, 1, 40);
		ImGui::Checkbox("Only activate animation hack on left mouse down", &animLeftDownOnly);
		ImGui::Text("");
		ImGui::Checkbox("F10 - Enable/Disable Mount Speed", &mountSpeedStart);
		ImGui::SliderInt("Mount Speed", &mountSpeed, 1200000, 25000000);
		ImGui::Text("");
		ImGui::Text("");
		ImGui::Text("");
		ImGui::Text("Made by floob");

		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void SendKey(short keyCode)
{
	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = keyCode;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;
	ip.ki.dwFlags = KEYEVENTF_SCANCODE;

	SendInput(1, &ip, sizeof(INPUT));
	Sleep(30);

	ip.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE;
	SendInput(1, &ip, sizeof(INPUT));
	Sleep(30);
}

LRESULT CALLBACK hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	POINT mPos;
	GetCursorPos(&mPos);
	ScreenToClient(window, &mPos);
	ImGui::GetIO().MousePos.x = mPos.x;
	ImGui::GetIO().MousePos.y = mPos.y;

	if (uMsg == WM_KEYUP)
	{
		if (wParam == VK_INSERT)
		{
			if (!ShowMenu)
			{
				SendKey(0x1D);
				io.MouseDrawCursor = false;
				SendKey(0x1D);
			}
			else
			{
				SendKey(0x1D);
				io.MouseDrawCursor = true;
			}
		}
	}

	if (ShowMenu)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	return CallWindowProc(OriginalWndProcHandler, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hookD3D11ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	imGuiInitializing = true;
	ImGui_ImplDX11_InvalidateDeviceObjects();
	CleanupRenderTarget();
	HRESULT toReturn = phookD3D11ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
	ImGui_ImplDX11_CreateDeviceObjects();
	imGuiInitializing = false;

	return toReturn;
}

HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		firstTime = false;

		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
		{
			SwapChain = pSwapChain;
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}

		DXGI_SWAP_CHAIN_DESC sd;
		pSwapChain->GetDesc(&sd);
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		window = sd.OutputWindow;

		OriginalWndProcHandler = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hWndProc);

		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX11_Init(pDevice, pContext);
		ImGui::GetIO().ImeWindowHandle = window;
	}

	if (RenderTargetView == NULL)
	{
		pContext->RSGetViewports(&vps, &viewport);
		ScreenCenterX = viewport.Width / 2.0f;
		ScreenCenterY = viewport.Height / 2.0f;

		ID3D11Texture2D* backbuffer = NULL;
		hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer);
		if (FAILED(hr))
		{
			return hr;
		}

		hr = pDevice->CreateRenderTargetView(backbuffer, NULL, &RenderTargetView);
		backbuffer->Release();
		if (FAILED(hr))
		{
			return hr;
		}
	}
	else
		pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);

	RenderMenu();

	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}



struct s_vector
{
	s_vector(float a1, float a2, float a3)
	{
		_x = a1;
		_y = a2;
		_z = a3;
	};
	void clear()
	{
		_x = 0;
		_y = 0;
		_z = 0;
	}
	bool is_zero()
	{
		if (_x == 0 && _y == 0 && _z == 0) return 1;
		else return 0;
	}
	float _x;
	float _y;
	float _z;
};

float relative_distance(s_vector start, s_vector end)
{
	if (start.is_zero() || end.is_zero()) return 0;
	auto _x_r = end._x - start._x;
	auto _y_r = end._y - start._y;
	auto _z_r = end._z - start._z;
	return static_cast<float>(std::sqrt(_x_r * _x_r + _y_r * _y_r + _z_r * _z_r));
}

void __stdcall hookD3D11Draw(ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation)
{
	return phookD3D11Draw(pContext, VertexCount, StartVertexLocation);
}

void SetAttackSpeed()
{
	uintptr_t attackSpeedAddress = GetDMA(localPlayer.base, &localPlayer.attackSpeed, 1);
	Write(attackSpeedAddress, attackSpeedVal);
}

void SetMovementSpeed()
{
	uintptr_t movementSpeedAdress = GetDMA(localPlayer.base, &localPlayer.moveSpeed, 1);
	Write(movementSpeedAdress, moveSpeedVal);
}

void SetCastSpeed()
{
	uintptr_t castSpeedAddress = GetDMA(localPlayer.base, &localPlayer.castSpeed, 1);
	Write(castSpeedAddress, castSpeedVal);
}

void SetAnimationSpeed()
{
	uintptr_t _characterControl = GetDMA(localPlayer.base, &localPlayer.characterControl, 1);
	uintptr_t _characterScene = GetDMA(_characterControl, &characterControl.characterScene, 1);
	uintptr_t _animationSpeedAddress = GetDMA(_characterScene, &characterScene.animationSpeed, 1);
	Write(_animationSpeedAddress, animationSpeedVal);
}

void ResetAnimationSpeed()
{
	uintptr_t _characterControl = GetDMA(localPlayer.base, &localPlayer.characterControl, 1);
	uintptr_t _characterScene = GetDMA(_characterControl, &characterControl.characterScene, 1);
	uintptr_t _animationSpeedAddress = GetDMA(_characterScene, &characterScene.animationSpeed, 1);
	Write(_animationSpeedAddress, 1.0f);
}


//void AutoLoot()
//{
//	if (lootReady)
//		takeLoot();
//
//	static auto lastTick = DWORD();
//	if (GetTickCount64() <= lastTick) return;
//	lastTick = GetTickCount64() + 150;
//
//	uintptr_t localP = Read_s<uint64_t>(localPlayer.base);
//	if (!localP) return;
//
//	lootReady = false;
//
//	uintptr_t type = 0x5C;
//	uintptr_t hasLoot = 0x3C8;
//	byte deadBody = 9;
//	uintptr_t actor = Read_s<uintptr_t>(localPlayer.base + 0x28);
//	if (!actor) return;
//
//	auto _type_actor = Read_s<byte>(actor + type);
//	if (_type_actor == deadBody) //!= 0
//	{
//		lootReady = true;
//		takeLoot();
//		requestLoot(localP, actor);
//		takeLoot();
//	}
//}

void __stdcall hookD3D11PSSetShaderResources(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	pssrStartSlot = StartSlot;

	if (GetAsyncKeyState(VK_F11) & 1)
	{
		if (speedStart || mountSpeedStart)
		{
			speedStart = false;
			mountSpeedStart = false;
		}
		else
		{
			speedStart = !speedStart;
			mountSpeedStart = !mountSpeedStart;
		}
	}

	if (GetAsyncKeyState(VK_F10) & 1)
	{
		mountSpeedStart = !mountSpeedStart;
	}

	if (GetAsyncKeyState(VK_F9) & 1)
	{
		speedStart = !speedStart;
	}

	if (GetAsyncKeyState(VK_INSERT) & 1)
	{
		ShowMenu = !ShowMenu;
	}

	if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_TAB) & 1)
		ShowMenu = false;
	if (GetAsyncKeyState(VK_TAB) && GetAsyncKeyState(VK_MENU) & 1)
		ShowMenu = false;

	return phookD3D11PSSetShaderResources(pContext, StartSlot, NumViews, ppShaderResourceViews);
}

void Teleport()
{
	uintptr_t _characterControl = GetDMA(localPlayer.base, &localPlayer.characterControl, 1);
	uintptr_t TeleportOffsetsX[] = { characterControl.teleport1, characterControl.teleport2, characterControl.teleport3 };
	uintptr_t TeleportOffsetsY[] = { characterControl.teleport1, characterControl.teleport2, characterControl.teleport3 + 0x4 };
	uintptr_t TeleportOffsetsZ[] = { characterControl.teleport1, characterControl.teleport2, characterControl.teleport3 + 0x8 };
	uintptr_t x = GetDMA(_characterControl, TeleportOffsetsX, 3);
	uintptr_t y = GetDMA(_characterControl, TeleportOffsetsY, 3);
	uintptr_t z = GetDMA(_characterControl, TeleportOffsetsZ, 3);
	float currentX = Read<float>(x);
	float currentY = Read<float>(y);
	float currentZ = Read<float>(z);

	uintptr_t c_x = GetDMA(localPlayer.base, &localPlayer.crossHairX, 1);
	uintptr_t c_y = GetDMA(localPlayer.base, &localPlayer.crossHairY, 1);
	uintptr_t c_z = GetDMA(localPlayer.base, &localPlayer.crossHairZ, 1);
	float currentCrossX = Read<float>(c_x);
	float currentCrossY = Read<float>(c_y);
	float currentCrossZ = Read<float>(c_z);

	float t = teleportSize;
	float xNew = (currentCrossX / 100 - currentX) * t + currentX;
	float yNew = (currentCrossY / 100 - currentY) * t + currentY;
	float zNew = (currentCrossZ / 100 - currentZ) * t + currentZ;


	Write(x, xNew);
	Write(y, yNew);
	Write(z, zNew);
}

void ClearPlayerSpeed()
{
	uintptr_t attackSpeedAddress = GetDMA(localPlayer.base, &localPlayer.attackSpeed, 1);
	Write(attackSpeedAddress, 300000);
	uintptr_t movementSpeedAdress = GetDMA(localPlayer.base, &localPlayer.moveSpeed, 1);
	Write(movementSpeedAdress, 300000);
	uintptr_t castSpeedAddress = GetDMA(localPlayer.base, &localPlayer.castSpeed, 1);
	Write(castSpeedAddress, 300000);
	uintptr_t _characterControl = GetDMA(localPlayer.base, &localPlayer.characterControl, 1);
	uintptr_t _characterScene = GetDMA(_characterControl, &characterControl.characterScene, 1);
	uintptr_t _animationSpeedAddress = GetDMA(_characterScene, &characterScene.animationSpeed, 1);
	Write(_animationSpeedAddress, 1.0f);
}

void SetMountSpeed()
{
	uintptr_t speed = GetDMA(localMount.base, &localMount.speed, 1);
	uintptr_t accel = GetDMA(localMount.base, &localMount.acceleration, 1);
	uintptr_t turn = GetDMA(localMount.base, &localMount.turn, 1);
	uintptr_t brake = GetDMA(localMount.base, &localMount.brake, 1);

	Write(speed, mountSpeed);
	Write(accel, mountSpeed);
	Write(turn, mountSpeed);
	Write(brake, mountSpeed);
}

void ClearMountSpeed()
{
	uintptr_t speed = GetDMA(localMount.base, &localMount.speed, 1);
	uintptr_t accel = GetDMA(localMount.base, &localMount.acceleration, 1);
	uintptr_t turn = GetDMA(localMount.base, &localMount.turn, 1);
	uintptr_t brake = GetDMA(localMount.base, &localMount.brake, 1);

	Write(speed, 1700000);
	Write(accel, 1700000);
	Write(turn, 1700000);
	Write(brake, 1700000);
}

void RecieveOffsets()
{
	localPlayer.moveSpeed = 0xB90;
	localPlayer.characterControl = 0x410;
	characterControl.characterScene = 0x10;
	characterScene.animationSpeed = 0x4C0;
	localMount.acceleration = 0x2AE8;
	/*localPlayer.crossHairX = (uintptr_t)std::stoi(offset, 0, 0);
	characterControl.teleport1 = (uintptr_t)std::stoi(offset, 0, 0);
	characterControl.teleport2 = (uintptr_t)std::stoi(offset, 0, 0);
	characterControl.teleport3 = (uintptr_t)std::stoi(offset, 0, 0);*/
					
	localPlayer.attackSpeed = localPlayer.moveSpeed + 0x4;
	localPlayer.castSpeed = localPlayer.moveSpeed + 0x8;

	/*localPlayer.crossHairY = localPlayer.crossHairX + 0x4;
	localPlayer.crossHairZ = localPlayer.crossHairX + 0x8;*/

	localMount.speed = localMount.acceleration + 0x4;
	localMount.turn = localMount.acceleration + 0x8;
	localMount.brake = localMount.acceleration + 0xC;
	
}

const int MultisampleCount = 1;
LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(hwnd, uMsg, wParam, lParam); }
DWORD __stdcall HookAndUpdate(LPVOID)
{
	RecieveOffsets();

#pragma region Hook
	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(4000);
	} while (!hDXGIDLL);
	Sleep(100);

	IDXGISwapChain* pSwapChain;

	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);
	HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

	D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
	D3D_FEATURE_LEVEL obtainedLevel;
	ID3D11Device* d3dDevice = nullptr;
	ID3D11DeviceContext* d3dContext = nullptr;

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(scd));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = MultisampleCount;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scd.Windowed = ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

	scd.BufferDesc.Width = 1;
	scd.BufferDesc.Height = 1;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;

	UINT createFlags = 0;

	IDXGISwapChain* d3dSwapChain = 0;

	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		requestedLevels,
		sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
		D3D11_SDK_VERSION,
		&scd,
		&pSwapChain,
		&pDevice,
		&obtainedLevel,
		&pContext)))
	{
		MessageBox(hWnd, "Failed to create directX device and swapchain!", "Error", MB_ICONERROR);
		return NULL;
	}


	pSwapChainVtable = (DWORD_PTR*)pSwapChain;
	pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

	pContextVTable = (DWORD_PTR*)pContext;
	pContextVTable = (DWORD_PTR*)pContextVTable[0];

	pDeviceVTable = (DWORD_PTR*)pDevice;
	pDeviceVTable = (DWORD_PTR*)pDeviceVTable[0];

	phookD3D11Present = (D3D11PresentHook)(DWORD_PTR*)pSwapChainVtable[8];
	phookD3D11ResizeBuffers = (D3D11ResizeBuffersHook)(DWORD_PTR*)pSwapChainVtable[13];
	phookD3D11PSSetShaderResources = (D3D11PSSetShaderResourcesHook)(DWORD_PTR*)pContextVTable[8];
	phookD3D11Draw = (D3D11DrawHook)(DWORD_PTR*)pContextVTable[13];

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(LPVOID&)phookD3D11Present, (PBYTE)hookD3D11Present);
	DetourAttach(&(LPVOID&)phookD3D11ResizeBuffers, (PBYTE)hookD3D11ResizeBuffers);
	DetourAttach(&(LPVOID&)phookD3D11PSSetShaderResources, (PBYTE)hookD3D11PSSetShaderResources);
	DetourAttach(&(LPVOID&)phookD3D11Draw, (PBYTE)hookD3D11Draw);
	DetourTransactionCommit();

	DWORD dwOld;
	VirtualProtect(phookD3D11Present, 2, PAGE_EXECUTE_READWRITE, &dwOld);
#pragma endregion

	GetAddresses();

	while (true)
	{
		if (speedStart)
		{
			if (!playerNopped)
			{
				NopAddress((PVOID)PlayerNOP, 8);
				playerNopped = true;
			}
			SetMovementSpeed();
			SetAttackSpeed();
			SetCastSpeed();
			if (!animLeftDownOnly)
				SetAnimationSpeed();
			else if (((GetKeyState(VK_LBUTTON) & 0x100) != 0))
				SetAnimationSpeed();
			else
				ResetAnimationSpeed();
		}
		else
		{
			ClearPlayerSpeed();
		}
		if (mountSpeedStart)
		{
			if (!mountNopped)
			{
				NopAddress((PVOID)MountNOP, 8);
				mountNopped = true;
			}
			SetMountSpeed();
		}
		else
		{
			ClearMountSpeed();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	pDevice->Release();
	pContext->Release();
	pSwapChain->Release();

	return NULL;
}

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(NULL, 0, HookAndUpdate, NULL, 0, NULL);
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
