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
	// ��������¼���ϵͳ��eve ����Ӣ�Ķ��ŷָ����ַ�������("flags,x,y,wheel")
	void sendMouseEvent(const char* eve, int len);
	// ���ͼ����¼���ϵͳ��eve ����Ӣ�Ķ��ŷָ����ַ�������("flags,key")
	void sendKeyEvent(const char* eve, int len);
	int find(const char* buf, int len, const char c);
	const char* find(const char* begin, const char* end, char c);

public:
	/*
	* ��������
	* buf ����Ӣ�Ķ��ŷָ����ַ������ݡ�
	* ���ݸ�ʽ�ǣ�������,�¼�����(���̡����),�¼����ݡ�("opcode,eve,eveData")
	* �ɹ������ѽ������¼���ʧ�ܷ�����Ӧ�Ĵ�����
	*/
	IN_EVENT_T parser(const char* buf, int len);
	// ��ȡ�µĲ�����
	const std::string& updateOpcode();
	// ��ȡ��ǰ������
	const std::string& getOpcode() const;
};

#endif // ! INPUT_MANAGER_H

