#include "GLWindow.h"

GLWindow::GLWindow(): m_hInstance(NULL), m_hWnd(NULL), m_hDC(NULL), m_hRC(NULL),
	m_width(0), m_height(0), m_fullScreen(false), m_active(false), m_running(false),
	m_renderProc(NULL)
{
}

GLWindow::~GLWindow()
{
	// ����������� ���������� ������
	if (m_fullScreen)
	{
		ChangeDisplaySettings(NULL, CDS_RESET);
		ShowCursor(TRUE);
	}

	// ������� �������� ����������
	if (m_hRC)
	{
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(m_hRC);
	}

	// ����������� �������� ����
	if (m_hDC)
		ReleaseDC(m_hWnd, m_hDC);

	// ������� ����
	if (m_hWnd)
		DestroyWindow(m_hWnd);

	// ������� ����� ����
	if (m_hInstance)
		UnregisterClass(GLWINDOW_CLASS_NAME, m_hInstance);
}

bool GLWindow::create(const char *title, int width, int height, bool fullScreen)
{
	ASSERT(title);
	ASSERT(width > 0);
	ASSERT(height > 0);

	WNDCLASSEX            wcx;
	PIXELFORMATDESCRIPTOR pfd;
	RECT                  rect;
	HGLRC                 hRCTemp;
	DWORD                 style, exStyle;
	int                   x, y, format;

	// ��������� ��������� �� ������� �������� ������������ ��������� OpenGL
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;

	// ������ �������� ��� �������� ������������ ��������� OpenGL
	int attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};

	m_hInstance = static_cast<HINSTANCE>(GetModuleHandle(NULL));

	// ����������� ������ ����
	memset(&wcx, 0, sizeof(wcx));
	wcx.cbSize        = sizeof(wcx);
	wcx.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wcx.lpfnWndProc   = reinterpret_cast<WNDPROC>(GLWindow::sWindowProc);
	wcx.hInstance     = m_hInstance;
	wcx.lpszClassName = GLWINDOW_CLASS_NAME;
	wcx.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wcx.hCursor       = LoadCursor(NULL, IDC_ARROW);

	if (!RegisterClassEx(&wcx))
	{
		LOG_ERROR("RegisterClassEx fail (%d)\n", GetLastError());
		return false;
	}

	// ����� ����
	style   = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	exStyle = WS_EX_APPWINDOW;

	// ��������� ���� �� ������ ������
	x = (GetSystemMetrics(SM_CXSCREEN) - width)  / 2;
	y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

	rect.left   = x;
	rect.right  = x + width;
	rect.top    = y;
	rect.bottom = y + height;

	// �������� ������ ���� ��� �����
	AdjustWindowRectEx (&rect, style, FALSE, exStyle);

	// ������� ����
	m_hWnd = CreateWindowEx(exStyle, GLWINDOW_CLASS_NAME, title, style, rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, m_hInstance, NULL);

	if (!m_hWnd)
	{
		LOG_ERROR("CreateWindowEx fail (%d)\n", GetLastError());
		return false;
	}

	// ������� ���������� ��������� ����
	m_hDC = GetDC(m_hWnd);

	if (!m_hDC)
	{
		LOG_ERROR("GetDC fail (%d)\n", GetLastError());
		return false;
	}

	// �������� ������� ��������
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize      = sizeof(pfd);
	pfd.nVersion   = 1;
	pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;

	// �������� ������ ��������, ��������� � ���������� ����
	format = ChoosePixelFormat(m_hDC, &pfd);
	if (!format || !SetPixelFormat(m_hDC, format, &pfd))
	{
		LOG_ERROR("Setting pixel format fail (%d)\n", GetLastError());
		return false;
	}

	// �������� ��������� �������� ����������
	// �� ����� ��� ��������� ������� wglCreateContextAttribsARB
	hRCTemp = wglCreateContext(m_hDC);
	if (!hRCTemp || !wglMakeCurrent(m_hDC, hRCTemp))
	{
		LOG_ERROR("�reating temp render context fail (%d)\n", GetLastError());
		return false;
	}

	// ������� ����� ������� ��������� ��������� ��������� ����������
	wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
		wglGetProcAddress("wglCreateContextAttribsARB"));

	if (!wglCreateContextAttribsARB)
	{
		LOG_ERROR("wglCreateContextAttribsARB fail (%d)\n", GetLastError());
		wglDeleteContext(hRCTemp);
		return false;
	}

	// �������� �������� ���������� ��� OpenGL 3.x
	m_hRC = wglCreateContextAttribsARB(m_hDC, 0, attribs);
	if (!m_hRC || !wglMakeCurrent(m_hDC, m_hRC))
	{
		LOG_ERROR("Creating render context fail (%d)\n", GetLastError());
		wglDeleteContext(hRCTemp);
		return false;
	}

	// ������� � ��� ������� ���������� � ��������� ����������
	int major, minor;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	LOG_DEBUG("OpenGL render context information:\n"
		"  Renderer       : %s\n"
		"  Vendor         : %s\n"
		"  Version        : %s\n"
		"  GLSL version   : %s\n"
		"  OpenGL version : %d.%d\n",
		(const char*)glGetString(GL_RENDERER),
		(const char*)glGetString(GL_VENDOR),
		(const char*)glGetString(GL_VERSION),
		(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION),
		major, minor
	);

	// ������ ��� ��������� �������� ���������� �� �����
	wglDeleteContext(hRCTemp);

	// ������� ��������� �� ���� � ���������� ���� ��� ���������� ������ windowProc
	SetProp(m_hWnd, GLWINDOW_PROP_NAME, reinterpret_cast<HANDLE>(this));
	// SetWindowLongPtr(m_hWnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));

	setSize(width, height, fullScreen);

	m_active = m_running = true;

	return true;
}

void GLWindow::setSize(int width, int height, bool fullScreen)
{
	ASSERT(width > 0);
	ASSERT(height > 0);

	RECT    rect;
	DWORD   style, exStyle;
	DEVMODE devMode;
	LONG    result;
	int     x, y;

	// ���� �� ������������ �� �������������� ������
	if (m_fullScreen && !fullScreen)
	{
		ChangeDisplaySettings(NULL, CDS_RESET);
		ShowCursor(TRUE);
	}

	m_fullScreen = fullScreen;

	// ���� ��������� ������������� �����
	if (m_fullScreen)
	{
		memset(&devMode, 0, sizeof(devMode));
		devMode.dmSize       = sizeof(devMode);
		devMode.dmPelsWidth  = width;
		devMode.dmPelsHeight = height;
		devMode.dmBitsPerPel = GetDeviceCaps(m_hDC, BITSPIXEL);
		devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

		// ������� ���������� ������������� �����
		result = ChangeDisplaySettings(&devMode, CDS_FULLSCREEN);
		if (result != DISP_CHANGE_SUCCESSFUL)
		{
			LOG_ERROR("ChangeDisplaySettings fail %dx%d (%d)\n", width, height, result);
			m_fullScreen = false;
		}
	}

	// ���� ��� �������� ������������� ����� � ��� ������� ����������
	if (m_fullScreen)
	{
		ShowCursor(FALSE);

		style   = WS_POPUP;
		exStyle = WS_EX_APPWINDOW | WS_EX_TOPMOST;

		x = y = 0;
	} else // ���� ������������� ����� �� �����, ��� ��� �� ������� ����������
	{
		style   = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		exStyle = WS_EX_APPWINDOW;

		// ��������� ���� �� ������ ������
		x = (GetSystemMetrics(SM_CXSCREEN) - width)  / 2;
		y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	}

	rect.left   = x;
	rect.right  = x + width;
	rect.top    = y;
	rect.bottom = y + height;

	// �������� ������ ���� ��� �����
	AdjustWindowRectEx (&rect, style, FALSE, exStyle);

	// ��������� ����� ����
	SetWindowLong(m_hWnd, GWL_STYLE,   style);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, exStyle);

	// ������� ������� ����
	SetWindowPos(m_hWnd, HWND_TOP, rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		SWP_FRAMECHANGED);

	// ������� ���� �� ������
	ShowWindow(m_hWnd, SW_SHOW);
	SetForegroundWindow(m_hWnd);
	SetFocus(m_hWnd);
	UpdateWindow(m_hWnd);

	// ������� ������� ����
	GetClientRect(m_hWnd, &rect);
	m_width  = rect.right - rect.left;
	m_height = rect.bottom - rect.top;

	// ������������� ������� �� ��� ����
	OPENGL_CALL(glViewport(0, 0, m_width, m_height));

	// ���������� ������ ������������ ����
	SetCursorPos(x + m_width / 2, y + m_height / 2);

	OPENGL_CHECK_FOR_ERRORS();
}

void GLWindow::setRenderProc(pRenderProc renderProc)
{
	m_renderProc = renderProc;
}

int GLWindow::mainLoop()
{
	MSG msg;

	// �������� ���� ����
	m_running = m_active = true;
	while (m_running)
	{
		// ���������� ��������� �� ������� ���������
		while (PeekMessage(&msg, m_hWnd, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				m_running = false;
				break;
			}
			// TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// ���� ���� � ������� ������ � �������
		if (m_running && m_active)
		{
			if (m_renderProc)
				m_renderProc(this);

			SwapBuffers(m_hDC);
		}

		Sleep(2);
	}

	m_running = m_active = false;
	return 0;
}

LRESULT CALLBACK GLWindow::windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE)
				m_running = false;

			if (wParam == VK_F1)
				setSize(m_width, m_height, !m_fullScreen);
		break;

		case WM_SETFOCUS:
		case WM_KILLFOCUS:
			m_active = (msg == WM_SETFOCUS);
		break;

		case WM_ACTIVATE:
			m_active = (!HIWORD(wParam));
		break;

		case WM_CLOSE:
		case WM_DESTROY:
			m_running = m_active = false;
			PostQuitMessage(0);
		break;

		case WM_SYSCOMMAND:
			switch (wParam)
			{
				case SC_SCREENSAVE:
				case SC_MONITORPOWER:
					return FALSE;
			}
		break;

		case WM_ERASEBKGND:
			return FALSE;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK GLWindow::sWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	GLWindow *window = reinterpret_cast<GLWindow*>(GetProp(hWnd, GLWINDOW_PROP_NAME));
	// GLWindow *window = reinterpret_cast<GLWindow*>(GetWindowLongPtr(hWnd, GWL_USERDATA));

	return window ? window->windowProc(hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
}
