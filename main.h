bool firstTime = true;
bool createdepthstencil = true;

UINT vps = 1;
D3D11_VIEWPORT viewport;
float ScreenCenterX;
float ScreenCenterY;

ID3D11Texture2D* texGreen = nullptr;
ID3D11Texture2D* texRed = nullptr;

ID3D11ShaderResourceView* texSRVgreen;
ID3D11ShaderResourceView* texSRVred;

ID3D11SamplerState* pSamplerState;
ID3D11SamplerState* pSamplerState2;

ID3D11RenderTargetView* RenderTargetView = NULL;

ID3D11RenderTargetView* pRTV[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
ID3D11DepthStencilView* pDSV;

ID3D11PixelShader* psRed = NULL;
ID3D11PixelShader* psGreen = NULL;

ID3D11Buffer* veBuffer;
UINT Stride;
UINT veBufferOffset;
D3D11_BUFFER_DESC vedesc;

ID3D11Buffer* inBuffer;
DXGI_FORMAT inFormat;
UINT        inOffset;
D3D11_BUFFER_DESC indesc;

UINT pscStartSlot;
ID3D11Buffer* pscBuffer;
D3D11_BUFFER_DESC pscdesc;

UINT vscStartSlot;
ID3D11Buffer* vscBuffer;
D3D11_BUFFER_DESC vscdesc;

UINT pssrStartSlot;
ID3D11Resource* Resource;
D3D11_SHADER_RESOURCE_VIEW_DESC Descr;
D3D11_TEXTURE2D_DESC texdesc;

void* ReturnAddress;

HWND window = nullptr;
bool ShowMenu = false;
static WNDPROC OriginalWndProcHandler = nullptr;

wchar_t reportValue[256];
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
HRESULT hr;
bool greetings = true;

struct propertiesModel
{
	UINT stride;
	UINT vedesc_ByteWidth;
	UINT indesc_ByteWidth;
	UINT pscdesc_ByteWidth;
};

uintptr_t PlayerNOP;
uintptr_t MountNOP;
uintptr_t RequestLoot;
uintptr_t TakeLoot;
uintptr_t NoCD;
uintptr_t FixTP;

int moveSpeedVal = 90000;
int attackSpeedVal = 90000;
int castSpeedVal = 90000;
int mountSpeed = 1200000;
float animationSpeedVal = 1;
float teleportSize = 0.01;

bool speedStart = false;
bool mountSpeedStart = false;
bool autoLootStart = false;
bool animLeftDownOnly = false;

bool playerNopped = false;
bool mountNopped = false;
bool lootingStart = false;
bool teleportStart = false;

int activeTimeLeft;
bool isTrial;

#define BUFFER_SIZE 1024

#pragma region offsets
BYTE Nop[] = { 0x90, 0x90, 0x90 , 0x90, 0x90, 0x90, 0x90 };

#pragma endregion

using namespace std;
#include <fstream>

void NopAddress(PVOID address, int bytes)
{
	DWORD d, ds;
	VirtualProtect(address, bytes, PAGE_EXECUTE_READWRITE, &d);
	memset(address, 0x90, bytes); VirtualProtect(address, bytes, d, &ds);
}

namespace std
{
	template<> struct hash<propertiesModel>
	{
		std::size_t operator()(const propertiesModel& obj) const noexcept
		{
			std::size_t h1 = std::hash<int>{}(obj.stride);
			std::size_t h2 = std::hash<int>{}(obj.vedesc_ByteWidth);
			std::size_t h3 = std::hash<int>{}(obj.indesc_ByteWidth);
			std::size_t h4 = std::hash<int>{}(obj.pscdesc_ByteWidth);
			return (h1 ^ h3 + h4) ^ (h2 << 1);
		}
	};
}

void ConsoleSetup()
{
	AllocConsole();
	FILE* file = nullptr;
	SetConsoleTitle("[+] ProBDO Console");
	freopen_s(&file, "CONOUT$", "w", stdout);
	freopen_s(&file, "CONOUT$", "w", stderr);
	freopen_s(&file, "CONIN$", "r", stdin);
}


int									g_Index = -1;
std::vector<void*>					g_Vector;
void* g_SelectedAddress = NULL;

bool IsAddressPresent(void* Address)
{
	for (auto it = g_Vector.begin(); it != g_Vector.end(); ++it)
	{
		if (*it == Address)
			return true;
	}
	return false;
}

HRESULT GenerateShader(ID3D11Device* pD3DDevice, ID3D11PixelShader** pShader, float r, float g, float b)
{
	char szCast[] = "struct VS_OUT"
		"{"
		" float4 Position : SV_Position;"
		" float4 Color : COLOR0;"
		"};"

		"float4 main( VS_OUT input ) : SV_Target"
		"{"
		" float4 fake;"
		" fake.a = 1.0f;"
		" fake.r = %f;"
		" fake.g = %f;"
		" fake.b = %f;"
		" return fake;"
		"}";

	ID3D10Blob* pBlob;
	char szPixelShader[1000];

	sprintf_s(szPixelShader, szCast, r, g, b);

	ID3DBlob* d3dErrorMsgBlob;

	HRESULT hr = D3DCompile(szPixelShader, sizeof(szPixelShader), "shader", NULL, NULL, "main", "ps_4_0", NULL, NULL, &pBlob, &d3dErrorMsgBlob);

	if (FAILED(hr))
		return hr;

	hr = pD3DDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, pShader);

	if (FAILED(hr))
		return hr;

	return S_OK;
}

//==========================================================================================================================

//orig
UINT stencilRef = 0;
D3D11_DEPTH_STENCIL_DESC origdsd;
ID3D11DepthStencilState* origDepthStencilState = NULL;

//wh
enum eDepthState
{
	ORIGINAL,
	ENABLED,
	DISABLED,
	READ_NO_WRITE,
	NO_READ_NO_WRITE,

	ENABLED1,
	ENABLED2,
	ENABLED3,
	ENABLED4,
	ENABLED5,
	ENABLED6,
	ENABLED7,
	ENABLED8,

	READ_NO_WRITE1,
	READ_NO_WRITE2,
	READ_NO_WRITE3,
	READ_NO_WRITE4,
	READ_NO_WRITE5,
	READ_NO_WRITE6,
	READ_NO_WRITE7,
	READ_NO_WRITE8,

	_DEPTH_COUNT
};

ID3D11DepthStencilState* myDepthStencilStates[static_cast<int>(eDepthState::_DEPTH_COUNT)];

void SetDepthStencilState(eDepthState aState)
{
	pContext->OMSetDepthStencilState(myDepthStencilStates[aState], 1);
}

char* state;
ID3D11RasterizerState* rDEPTHBIASState;
ID3D11RasterizerState* rNORMALState;
ID3D11RasterizerState* rWIREFRAMEState;
ID3D11RasterizerState* rSOLIDState;

#include <string>
#include <fstream>
//save cfg
void SaveCfg()
{
	ofstream fout;
	//fout.open(GetDirectoryFile("d3dwh.ini"), ios::trunc);
	//fout << "Example " << Example << endl;
	fout.close();
}

//load cfg
void LoadCfg()
{
	ifstream fin;
	string Word = "";
	//fin.open(GetDirectoryFile("d3dwh.ini"), ifstream::in);
	//fin >> Word >> Example;
	fin.close();
}

void CreateRenderTarget()
{
	DXGI_SWAP_CHAIN_DESC sd;
	SwapChain->GetDesc(&sd);
	ID3D11Texture2D* pBackBuffer;
	D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
	ZeroMemory(&render_target_view_desc, sizeof(render_target_view_desc));
	render_target_view_desc.Format = sd.BufferDesc.Format;
	render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	hr = pDevice->CreateRenderTargetView(pBackBuffer, &render_target_view_desc, &RenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (nullptr != RenderTargetView)
	{
		RenderTargetView->Release();
		RenderTargetView = nullptr;
	}
}

void CreateDepthStencilStates()
{
	D3D11_DEPTH_STENCIL_DESC  stencilDesc;
	stencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	stencilDesc.StencilEnable = true;
	stencilDesc.StencilReadMask = 0xFF;
	stencilDesc.StencilWriteMask = 0xFF;
	stencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	stencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	stencilDesc.DepthEnable = true;
	stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED)]);

	stencilDesc.DepthEnable = false;
	stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::DISABLED)]);

	stencilDesc.DepthEnable = false;
	stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDesc.StencilEnable = false;
	stencilDesc.StencilReadMask = UINT8(0xFF);
	stencilDesc.StencilWriteMask = 0x0;
	pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::NO_READ_NO_WRITE)]);

	stencilDesc.DepthEnable = true;
	stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	stencilDesc.StencilEnable = false;
	stencilDesc.StencilReadMask = UINT8(0xFF);
	stencilDesc.StencilWriteMask = 0x0;

	stencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

	stencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
	pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE)]);


	D3D11_DEPTH_STENCIL_DESC stencilDesc1;
	stencilDesc1.DepthFunc = D3D11_COMPARISON_NEVER;
	stencilDesc1.StencilEnable = true;
	stencilDesc1.StencilReadMask = 0xFF;
	stencilDesc1.StencilWriteMask = 0xFF;
	stencilDesc1.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc1.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc1.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc1.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
	stencilDesc1.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc1.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc1.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc1.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
	stencilDesc1.DepthEnable = true;
	stencilDesc1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc1, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED1)]);

	D3D11_DEPTH_STENCIL_DESC stencilDesc2;
	stencilDesc2.DepthFunc = (D3D11_COMPARISON_FUNC)2;
	stencilDesc2.StencilEnable = true;
	stencilDesc2.StencilReadMask = 0xFF;
	stencilDesc2.StencilWriteMask = 0xFF;
	stencilDesc2.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc2.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc2.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc2.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	stencilDesc2.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc2.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc2.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc2.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	stencilDesc2.DepthEnable = true;
	stencilDesc2.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc2, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED2)]);

	D3D11_DEPTH_STENCIL_DESC stencilDesc3;
	stencilDesc3.DepthFunc = (D3D11_COMPARISON_FUNC)3;
	stencilDesc3.StencilEnable = true;
	stencilDesc3.StencilReadMask = 0xFF;
	stencilDesc3.StencilWriteMask = 0xFF;
	stencilDesc3.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc3.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc3.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc3.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
	stencilDesc3.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc3.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc3.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc3.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
	stencilDesc3.DepthEnable = true;
	stencilDesc3.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc3, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED3)]);

	D3D11_DEPTH_STENCIL_DESC stencilDesc4;
	stencilDesc4.DepthFunc = (D3D11_COMPARISON_FUNC)4;
	stencilDesc4.StencilEnable = true;
	stencilDesc4.StencilReadMask = 0xFF;
	stencilDesc4.StencilWriteMask = 0xFF;
	stencilDesc4.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc4.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc4.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc4.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
	stencilDesc4.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc4.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc4.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc4.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
	stencilDesc4.DepthEnable = true;
	stencilDesc4.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc4, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED4)]);

	D3D11_DEPTH_STENCIL_DESC stencilDesc5;
	stencilDesc5.DepthFunc = (D3D11_COMPARISON_FUNC)5;
	stencilDesc5.StencilEnable = true;
	stencilDesc5.StencilReadMask = 0xFF;
	stencilDesc5.StencilWriteMask = 0xFF;
	stencilDesc5.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc5.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc5.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc5.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
	stencilDesc5.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc5.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc5.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc5.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
	stencilDesc5.DepthEnable = true;
	stencilDesc5.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc5, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED5)]);

	D3D11_DEPTH_STENCIL_DESC stencilDesc6;
	stencilDesc6.DepthFunc = (D3D11_COMPARISON_FUNC)6;
	stencilDesc6.StencilEnable = true;
	stencilDesc6.StencilReadMask = 0xFF;
	stencilDesc6.StencilWriteMask = 0xFF;
	stencilDesc6.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc6.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc6.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc6.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
	stencilDesc6.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc6.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc6.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc6.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
	stencilDesc6.DepthEnable = true;
	stencilDesc6.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc6, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED6)]);

	D3D11_DEPTH_STENCIL_DESC stencilDesc7;
	ZeroMemory(&stencilDesc7, sizeof(D3D11_DEPTH_STENCIL_DESC));
	stencilDesc7.DepthFunc = (D3D11_COMPARISON_FUNC)7;
	stencilDesc7.StencilEnable = true;
	stencilDesc7.StencilReadMask = 0xFF;
	stencilDesc7.StencilWriteMask = 0xFF;
	stencilDesc7.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc7.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc7.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc7.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
	stencilDesc7.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc7.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc7.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc7.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
	stencilDesc7.DepthEnable = true;
	stencilDesc7.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc7, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED7)]);

	D3D11_DEPTH_STENCIL_DESC stencilDesc8;
	ZeroMemory(&stencilDesc8, sizeof(D3D11_DEPTH_STENCIL_DESC));
	stencilDesc8.DepthFunc = (D3D11_COMPARISON_FUNC)8;
	stencilDesc8.StencilEnable = true;
	stencilDesc8.StencilReadMask = 0xFF;
	stencilDesc8.StencilWriteMask = 0xFF;
	stencilDesc8.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc8.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	stencilDesc8.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc8.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
	stencilDesc8.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc8.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	stencilDesc8.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDesc8.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
	stencilDesc8.DepthEnable = true;
	stencilDesc8.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	pDevice->CreateDepthStencilState(&stencilDesc8, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED8)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW1;
	stencilDescRNW1.DepthEnable = true;
	stencilDescRNW1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW1.DepthFunc = D3D11_COMPARISON_NEVER;
	stencilDescRNW1.StencilEnable = false;
	stencilDescRNW1.StencilReadMask = UINT8(0xFF);
	stencilDescRNW1.StencilWriteMask = 0x0;
	stencilDescRNW1.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW1.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW1.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW1.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
	stencilDescRNW1.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW1.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW1.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW1.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
	pDevice->CreateDepthStencilState(&stencilDescRNW1, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE1)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW2;
	stencilDescRNW2.DepthEnable = true;
	stencilDescRNW2.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW2.DepthFunc = (D3D11_COMPARISON_FUNC)2;
	stencilDescRNW2.StencilEnable = false;
	stencilDescRNW2.StencilReadMask = UINT8(0xFF);
	stencilDescRNW2.StencilWriteMask = 0x0;
	stencilDescRNW2.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW2.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW2.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW2.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	stencilDescRNW2.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW2.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW2.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW2.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
	pDevice->CreateDepthStencilState(&stencilDescRNW2, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE2)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW3;
	stencilDescRNW3.DepthEnable = true;
	stencilDescRNW3.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW3.DepthFunc = (D3D11_COMPARISON_FUNC)3;
	stencilDescRNW3.StencilEnable = false;
	stencilDescRNW3.StencilReadMask = UINT8(0xFF);
	stencilDescRNW3.StencilWriteMask = 0x0;
	stencilDescRNW3.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW3.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW3.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW3.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
	stencilDescRNW3.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW3.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW3.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW3.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
	pDevice->CreateDepthStencilState(&stencilDescRNW3, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE3)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW4;
	stencilDescRNW4.DepthEnable = true;
	stencilDescRNW4.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW4.DepthFunc = (D3D11_COMPARISON_FUNC)4;
	stencilDescRNW4.StencilEnable = false;
	stencilDescRNW4.StencilReadMask = UINT8(0xFF);
	stencilDescRNW4.StencilWriteMask = 0x0;
	stencilDescRNW4.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW4.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW4.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW4.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
	stencilDescRNW4.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW4.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW4.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW4.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
	pDevice->CreateDepthStencilState(&stencilDescRNW4, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE4)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW5;
	stencilDescRNW5.DepthEnable = true;
	stencilDescRNW5.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW5.DepthFunc = (D3D11_COMPARISON_FUNC)5;
	stencilDescRNW5.StencilEnable = false;
	stencilDescRNW5.StencilReadMask = UINT8(0xFF);
	stencilDescRNW5.StencilWriteMask = 0x0;
	stencilDescRNW5.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW5.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW5.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW5.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
	stencilDescRNW5.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW5.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW5.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW5.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
	pDevice->CreateDepthStencilState(&stencilDescRNW5, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE5)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW6;
	stencilDescRNW6.DepthEnable = true;
	stencilDescRNW6.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW6.DepthFunc = (D3D11_COMPARISON_FUNC)6;
	stencilDescRNW6.StencilEnable = false;
	stencilDescRNW6.StencilReadMask = UINT8(0xFF);
	stencilDescRNW6.StencilWriteMask = 0x0;
	stencilDescRNW6.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW6.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW6.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW6.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
	stencilDescRNW6.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW6.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW6.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW6.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
	pDevice->CreateDepthStencilState(&stencilDescRNW6, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE6)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW7;
	stencilDescRNW7.DepthEnable = true;
	stencilDescRNW7.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW7.DepthFunc = (D3D11_COMPARISON_FUNC)7;
	stencilDescRNW7.StencilEnable = false;
	stencilDescRNW7.StencilReadMask = UINT8(0xFF);
	stencilDescRNW7.StencilWriteMask = 0x0;
	stencilDescRNW7.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW7.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW7.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW7.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
	stencilDescRNW7.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW7.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW7.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW7.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
	pDevice->CreateDepthStencilState(&stencilDescRNW7, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE7)]);

	D3D11_DEPTH_STENCIL_DESC stencilDescRNW8;
	stencilDescRNW8.DepthEnable = true;
	stencilDescRNW8.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	stencilDescRNW8.DepthFunc = (D3D11_COMPARISON_FUNC)8;
	stencilDescRNW8.StencilEnable = false;
	stencilDescRNW8.StencilReadMask = UINT8(0xFF);
	stencilDescRNW8.StencilWriteMask = 0x0;
	stencilDescRNW8.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW8.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW8.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	stencilDescRNW8.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
	stencilDescRNW8.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW8.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW8.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
	stencilDescRNW8.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
	pDevice->CreateDepthStencilState(&stencilDescRNW8, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE8)]);
}