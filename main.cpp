#include <nil/window.hpp>

#include <windows.h>

#include "console.hpp"

namespace
{
	console main_console;
	HINSTANCE instance_handle;
}

LRESULT CALLBACK window_procedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
			main_console.initialise(hWnd);
			break;

		case WM_DESTROY:
			PostQuitMessage(WM_QUIT);
			break;

		case WM_ERASEBKGND:
			return 1;

		case WM_SYSKEYUP:
			return 1;

		case WM_CHAR:
			main_console.input(static_cast<unsigned>(wParam));
			break;

		case WM_KEYDOWN:
			main_console.key_down(static_cast<unsigned>(wParam));
			break;

		case WM_LBUTTONDOWN:
			main_console.left_mouse_button_down(static_cast<unsigned>(LOWORD(lParam)), static_cast<unsigned>(HIWORD(lParam)));
			break;

		case WM_LBUTTONUP:
			main_console.left_mouse_button_up(static_cast<unsigned>(LOWORD(lParam)), static_cast<unsigned>(HIWORD(lParam)));
			break;

		case WM_RBUTTONDOWN:
			main_console.right_mouse_button_down(static_cast<unsigned>(LOWORD(lParam)), static_cast<unsigned>(HIWORD(lParam)));
			break;

		case WM_SETCURSOR:
		{
			POINT position;
			GetCursorPos(&position);
			ScreenToClient(hWnd, &position);
			main_console.mouse_move(static_cast<unsigned>(position.x), static_cast<unsigned>(position.y));
			break;
		}

		case WM_MOUSEWHEEL:
			main_console.mouse_wheel(GET_WHEEL_DELTA_WPARAM(wParam));
			break;

		case WM_SIZE:
			main_console.resize();
			break;

		case WM_PAINT:
			main_console.draw();
			break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	std::string const name = "caqypowu";
	instance_handle = hInstance;
	HWND window_handle = nil::create_window(name, name, nil::screen.width / 2, nil::screen.height / 2, &window_procedure, hInstance); 
	while(nil::get_message(window_handle));
	return 0;
}
