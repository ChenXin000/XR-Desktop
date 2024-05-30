#ifndef INPUT_H
#define INPUT_H

#include <windows.h>
#include <winuser.h>
#include <unordered_map>

class Input
{
public:
	explicit Input();
	~Input();

private:
	std::unordered_map<unsigned short, bool> keyMarked;

public:
	void keyDown(unsigned short key);
	void keyUp(unsigned short key);

	void keyboardEvent(long flags, unsigned short key);
	void mouseEvent(long flags, long x, long y, long wheel);

	void releaseKey();
	void releaseMouse();
};

inline Input::Input()
{

}

inline Input::~Input()
{
	releaseKey();
	releaseMouse();
}

inline void Input::keyDown(unsigned short key)
{
	auto it = keyMarked.find(key);
	if (it != keyMarked.end() && it->second)
		keyUp(key); // 如果已按下则释放重新按下

	INPUT input;
	memset(&input, 0, sizeof(INPUT));
	input.type = INPUT_KEYBOARD;
	input.ki.wScan = key;
	input.ki.dwFlags = KEYEVENTF_SCANCODE;
	// 扩展键标志 0xE0
	if ((key >> 8 & 0xFF) == 0xE0)
		input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

	if (SendInput(1, &input, sizeof(INPUT)))
		keyMarked[key] = true; // 标记为按下
}

inline void Input::keyUp(unsigned short key)
{
	INPUT input;
	memset(&input, 0, sizeof(INPUT));
	input.type = INPUT_KEYBOARD;
	input.ki.wScan = key;
	input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
	// 扩展键标志 0xE0
	if ((key >> 8 & 0xFF) == 0xE0)
		input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

	if(SendInput(1, &input, sizeof(INPUT)))
		keyMarked[key] = false; // 标记为释放
}

inline void Input::keyboardEvent(long flags, unsigned short key)
{
	if (flags == KEYEVENTF_KEYUP)
		keyUp(key);
	else keyDown(key);
}

inline void Input::mouseEvent(long flags, long x, long y, long wheel)
{
	INPUT input;
	memset(&input, 0, sizeof(INPUT));

	input.type = INPUT_MOUSE;
	input.mi.dwFlags = flags;
	input.mi.mouseData = wheel;
	input.mi.dx = x;
	input.mi.dy = y;

	SendInput(1, &input, sizeof(INPUT));
}

inline void Input::releaseKey()
{
	auto it = keyMarked.begin();
	auto end = keyMarked.end();

	while (it != end)
	{
		if (it->second)
			keyUp(it->first);
		it++;
	}
}

inline void Input::releaseMouse()
{
	INPUT input;
	memset(&input, 0, sizeof(INPUT));
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = (MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_MIDDLEUP);

	SendInput(1, &input, sizeof(INPUT));
}

#endif // !INPUT_H
