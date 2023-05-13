#include <Windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>

//--------------------------------------------------------------------------------
// Структуры
//--------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
};

//--------------------------------------------------------------------------------
// Глобальные переменные
//--------------------------------------------------------------------------------
HINSTANCE					g_hInst = NULL;
HWND						g_hWnd = NULL;
D3D_DRIVER_TYPE				g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL			g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = NULL;					// Устройство (для создания объектов)
ID3D11DeviceContext* g_pImmediateContext = NULL;	// Контекст устройства (рисование)
IDXGISwapChain* g_pSwapChain = NULL;				// Цепь связи (буфер с экраном)
ID3D11RenderTargetView* g_pRenderTargetView = NULL; // Объект заднего буфера
ID3D11VertexShader* g_pVertexShader = NULL;			// Вершинный шейдер
ID3D11PixelShader* g_pPixelShader = NULL;			// Пиксельный шейдер
ID3D11InputLayout* g_pVertexLayout = NULL;			// Описание формата вершин
ID3D11Buffer* g_pVertexBuffer = NULL;				// Буфер вершин

//--------------------------------------------------------------------------------
// Предварительное объявление функций
//--------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int mCmdShow);	// Создание окна
HRESULT InitDevice();									// Инициализация устройств DirectX
HRESULT InitGeometry();									// Инициализация шаблона ввода и буфера вершин
void CleanupDevice();									// Удаление созданных устройсв DirectX
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);	// Функция окна
void Render();											// Функция рисования

//--------------------------------------------------------------------------------
// Точка входа в программу. Здесь мы все инициализируем и входим в цикл сообщений.
// Время простоя используем для вызова функции рисования
//--------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int mCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Создание окна приложения
	if (FAILED(InitWindow(hInstance, mCmdShow))) // Если окно не создано - вернуть 0
		return 0;

	// Создание объектов DirectX
	if (FAILED(InitDevice())) // Если устройство не инициализировано - очистить устройство и вернуть 0
	{
		CleanupDevice();
		return 0;
	}

	// Создание шейдеров и буфера вершин
	if (FAILED(InitGeometry()))
	{
		CleanupDevice();
		return 0;
	}

	// Главный цикл сообщений
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // Если сообщения есть - передавать их в обработчик
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else // Если сообщений нет
		{
			Render();
		}
	}

	CleanupDevice();
	return (int)msg.wParam;
}

//--------------------------------------------------------------------------------
// Регистрация класса и создание окна
//--------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Регистрация класса
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"Kurs3WindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDC_ICON);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Создание окна
	g_hInst = hInstance;
	RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"Kurs3WindowClass", L"Курсовая работа Васильева В. А.", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);
	return S_OK;
}

//--------------------------------------------------------------------------------
// Вызывается каждый раз, когда приложение получает сообщение
//--------------------------------------------------------------------------------
LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//--------------------------------------------------------------------------------
// Инициализация устройства DirectX
//--------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;	// Получаем ширину
	UINT height = rc.bottom - rc.top;	// Получаем высоту
	UINT createDeviceFlags = 0;

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE
	};									// Список различных драйверов, которые расположены в порядке возрастания вероятности 
	// их поддержки на ПК пользователя
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	//Создаем аналогичный список поддерживаемых версий DirectX
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	// Создание устройства DirectX. Для начала заполняем структуру, которая описывает свойства переднего буфера и привязывает его к нашему окну
	DXGI_SWAP_CHAIN_DESC sd;							// Структура, описывающая цепь связи (Swap Chain)
	ZeroMemory(&sd, sizeof(sd));						// Очистка
	sd.BufferCount = 1;									// 1 задний буфер
	sd.BufferDesc.Width = width;						// Ширина буфера
	sd.BufferDesc.Height = height;						// Высота буфера
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// Формат пикселя в буфере
	sd.BufferDesc.RefreshRate.Numerator = 75;			// Частота обновления экрана
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// Назначение буфера - задний буфер
	sd.OutputWindow = g_hWnd;							// Призываем к созданному окну
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;									// Оконный режим
	// Создаем цикл для того, чтобы перебрать поддерживаемые драйвера
	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd,
			&g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))	// Если устройства созданы успешно, то выходим из цикла 
			break;
	}
	if (FAILED(hr))
		return hr;
	// Создание заднего буфера
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Подключаем объект заднего буфера к контексту устройства
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	// Настройка вьюпорта. Указываем что левый верхний угол будет иметь координаты (0; 0)
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	// Подключаем вьюпорт к контексту устройства
	g_pImmediateContext->RSSetViewports(1, &vp);

	return S_OK;
}

//--------------------------------------------------------------------------------
// Создание буфера вершин, шейдеров и описание формата вершин
//--------------------------------------------------------------------------------
HRESULT InitGeometry()
{
	HRESULT hr = S_OK;

	// Компиляция вершинного шейдера из файла
	ID3DBlob* pVSBlob = NULL;
	wchar_t filename[] = L"kurs.fx";
	hr = CompileShaderFromFile(filename, "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL, L"Невозможно скомпилировать файл FX. Пожалуйста, запустите данную программу из папки, содержащей файл FX.", 
			L"Ошибка", MB_OK);
		return hr;
	}

	// Создание вершинного шейдера
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Определение шаблона вершин
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		/* семантическое имя, семантический индекс, размер, входящий слот(0-15), адрес начала данных в буфере вершин, класс входящего слота, InstanceDataStepRate*/
	};
	UINT numElements = ARRAYSIZE(layout);

	// Создание шаблона вершин
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Подключение шаблона вершин
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// Компиляция пиксельного шейдера из файла
	ID3DBlob* pPSBlob = NULL;
	wchar_t filename[] = L"kurs.fx";
	hr = CompileShaderFromFile(filename, "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL, L"Невозможно скомпилировать файл FX. Пожалуйста, запустите данную программу из папки, содержащей файл FX.",
			L"Ошибка", MB_OK);
		return hr;
	}

	// Создание пиксельного шейдера
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Создание буфера вершин
	SimpleVertex vertices[3];

	vertices[0].Pos.x = 0.0f;	vertices[0].Pos.y = 0.5f;	vertices[0].Pos.z = 0.5f;
	vertices[1].Pos.x = 0.5f;	vertices[1].Pos.y = -0.5f;	vertices[1].Pos.z = 0.5f;
	vertices[2].Pos.x = -0.5f;	vertices[2].Pos.y = -0.5f;	vertices[2].Pos.z = 0.5f;

	D3D11_BUFFER_DESC bd;						// Структура, описывающая создаваемый буфер
	ZeroMemory(&bd, sizeof(bd));				// Очитска структуры
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 3;	// Размер буфера = размер одной вершины * 3
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// Тип буфера - буфер вершин
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;			// Структура содержащая данные буфера
	ZeroMemory(&InitData, sizeof(InitData));	// Очистка структуры
	InitData.pSysMem = vertices;				// Указатель на вершины

	// Создание объекта буфера
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;

	// Установка буфера вершин
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Установка способа отрисовки вершин в буфере
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

//--------------------------------------------------------------------------------
// Удалить все созданные объекты
//--------------------------------------------------------------------------------
void CleanupDevice()
{
	// Сначала отключаем контекст устройства, затем очищаем все остальное
	if (g_pImmediateContext)
		g_pImmediateContext->ClearState();
	// Объекты удаляются в порядке, обратном тому, как они создавались
	if (g_pVertexBuffer)
		g_pVertexBuffer->Release();
	if (g_pVertexLayout)
		g_pVertexLayout->Release();
	if (g_pVertexShader)
		g_pVertexShader->Release();
	if (g_pPixelShader)
		g_pPixelShader->Release();
	if (g_pRenderTargetView)
		g_pRenderTargetView->Release();
	if (g_pSwapChain)
		g_pSwapChain->Release();
	if (g_pImmediateContext)
		g_pImmediateContext->Release();
	if (g_pd3dDevice)
		g_pd3dDevice->Release();
}

//--------------------------------------------------------------------------------
// Рисование кадра
//--------------------------------------------------------------------------------
void Render()
{
	// Очищаем задний буфер
	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };	// Красный, зеленый, синий, альфа-канал
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

	// Подключить к устройству рисования шейдеры
	g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

	// Нарисовать три вершины
	g_pImmediateContext->Draw(3, 0);

	// Отображение заднего буфера на экране
	g_pSwapChain->Present(0, 0);
}

//--------------------------------------------------------------------------------
// Вспомогательная функция для компиляции шейдеров в D3DX11
//--------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
	ID3DBlob* pErrorBlob;

	hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel, dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob)
			pErrorBlob->Release();
		return hr;
	}
	if (pErrorBlob)
		pErrorBlob->Release();

	return S_OK;
}