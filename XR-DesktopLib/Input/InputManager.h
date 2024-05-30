#ifndef  INPUT_MANAGER_H
#define INPUT_MARAGER_H

#include <string>
#include "Input.h"
#include <random>

enum IN_EVENT_T
{
	IN_NONE,
	IN_ERROR,
	IN_KEYBOARD_EVENT	= 'k',
	IN_MOUSE_EVENT		= 'm',
	IN_OPCODE_CHECK		= 'o',
	IN_OPCODE_ERROR,
	IN_OPCODE_OK,
};

class InputManager
{
public:
	const char OPCODE_LENGHT = 6;

public:
	explicit InputManager();
	~InputManager();

private:
	std::string opcode;
	Input input;

	bool checkOpcode(const char* opc);
	// 发送鼠标事件给系统，eve 是以英文逗号分隔的字符串数据("flags,x,y,wheel")
	void sendMouseEvent(const char* eve, int len);
	// 发送键盘事件给系统，eve 是以英文逗号分隔的字符串数据("flags,key")
	void sendKeyEvent(const char* eve, int len);
	int find(const char* buf, int len, const char c);
	const char* find(const char* begin, const char* end, char c);

public:
	/*
	* 解析数据
	* buf 是以英文逗号分隔的字符串数据。
	* 数据格式是：操作码,事件类型(键盘、鼠标),事件数据。("opcode,eve,eveData")
	* 成功返回已解析的事件，失败返回相应的错误码
	*/
	IN_EVENT_T parser(const char* buf, int len);
	// 获取新的操作码
	const std::string& updateOpcode();
	// 获取当前操作码
	const std::string& getOpcode() const;
};

#endif // ! INPUT_MANAGER_H

