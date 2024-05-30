#ifndef SINK_H
#define SINK_H

#include "Source.h"

class Sink
{
public:
	explicit Sink(const std::string& type) : mediaType(type) {}
	~Sink() { breakLink(); }

	const std::string& getMediaType() const { return mediaType; }; // ��ȡ����ý������
	// ����������򷵻ش��󣬱����ȶϿ����ӡ�
	bool linkSource(Source& src) {
		if (isLinkSource()) return false;

		if(src.getMediaType() == mediaType)
			source = &src;
		return isLinkSource();
	};
	bool isLinkSource() const { return source; };
	// �ڶϿ�����ʱҪȷ���������ж� source ���Եĵ��ã�������ֿ�ָ�롣
	void breakLink() { source = nullptr; };

private:
	std::string mediaType;

protected:
	Source* source = nullptr;
};


#endif // !SINK_H
