#ifndef SINK_H
#define SINK_H

#include "Source.h"

class Sink
{
public:
	explicit Sink(const std::string& type) : mediaType(type) {}
	~Sink() { breakLink(); }

	const std::string& getMediaType() const { return mediaType; }; // 获取数据媒体类型
	// 如果已连接则返回错误，必须先断开连接。
	bool linkSource(Source& src) {
		if (isLinkSource()) return false;

		if(src.getMediaType() == mediaType)
			source = &src;
		return isLinkSource();
	};
	bool isLinkSource() const { return source; };
	// 在断开连接时要确保结束所有对 source 属性的调用，否则出现空指针。
	void breakLink() { source = nullptr; };

private:
	std::string mediaType;

protected:
	Source* source = nullptr;
};


#endif // !SINK_H
