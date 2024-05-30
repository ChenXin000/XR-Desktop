#include "InputManager.h"

InputManager::InputManager() 
{
	updateOpcode();
}

InputManager::~InputManager()
{

}
// ��������
bool InputManager::checkOpcode(const char* opc)
{
	return !memcmp(opc, opcode.c_str(), OPCODE_LENGHT);
}
// ��������¼�
void InputManager::sendMouseEvent(const char* eve, int len)
{
	const char* end = eve + len;
	long values[4] = { 0,0,0,0 };
	int i = 0;
	do {
		values[i++] = std::atol(eve);
		eve = find(eve, end, ',') + 1;
	} while (eve != end && i < 4);

	input.mouseEvent(values[0],values[1],values[2],values[3]);
}
// ���ͼ����¼�
void InputManager::sendKeyEvent(const char* eve, int len)
{
	const char* end = eve + len;
	long values[2] = { 0,0 };
	int i = 0;
	do {
		values[i++] = std::atol(eve);
		eve = find(eve, end, ',') + 1;
	} while (eve != end && i < 2);

	input.keyboardEvent(values[0], static_cast<unsigned short>(values[1]));
}
// �����ַ�
int InputManager::find(const char* buf, int len, const char c)
{
	int i = 0;
	while(i < len)
	{
		if (buf[i] == c) 
			return i;
		++i;
	}
	return i;
}
// �����ַ� c, �ɹ������ַ�����λ�õ�ָ�룬ʧ�ܷ��� end��
const char* InputManager::find(const char* begin, const char* end, char c)
{
	while (begin != end)
	{
		if (*begin == c)
			break;
		++begin;
	}
	return begin;
}
// ��ȡ������
const std::string& InputManager::getOpcode() const
{
	return opcode;
}
// ���²�����
const std::string& InputManager::updateOpcode()
{
	std::random_device rd;   // non-deterministic generator
	std::mt19937 gen(rd());  // to seed mersenne twister.
	std::uniform_int_distribution<> dist('a', 'z'); // distribute results between 1 and 6 inclusive.
	opcode.clear();
	for (int i = 0; i < OPCODE_LENGHT; ++i) {
		opcode.push_back(dist(gen)); // pass the generator to the distribution.
	}
	return opcode;
}
// ��������
IN_EVENT_T InputManager::parser(const char* buf, int len)
{
	if (len < OPCODE_LENGHT + 2) return IN_ERROR;
	if (!checkOpcode(buf)) return IN_OPCODE_ERROR;

	char type = buf[OPCODE_LENGHT + 1];
	// ������������¼�����
	len -= (OPCODE_LENGHT + 3);
	if (len < 1) return IN_ERROR;
	buf += (OPCODE_LENGHT + 3);

	switch (type)
	{
	case IN_MOUSE_EVENT:
		sendMouseEvent(buf, len);
		return IN_MOUSE_EVENT;

	case IN_KEYBOARD_EVENT:
		sendKeyEvent(buf, len);
		return IN_KEYBOARD_EVENT;

	case IN_OPCODE_CHECK:
		return IN_OPCODE_OK;
	}

	return IN_NONE;
}
