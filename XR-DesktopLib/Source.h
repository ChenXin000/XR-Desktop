#ifndef SOURCE_H
#define SOURCE_H

#include "gst/gst.h"
#include <string>

class Source
{
public:
	explicit Source(const std::string& type) : mediaType(type) {};

	virtual GstSample* pullSample() = 0;
	const std::string& getMediaType() const { return mediaType; }; // ��ȡ����ý������
	Source& getSource() { return *this; };

private:
	std::string mediaType;
};


#endif // !SOURCE_H



