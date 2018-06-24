#pragma once
#ifndef LLVMSQL_BUFFER_H
#define LLVMSQL_BUFFER_H
#include"..\catalog\catalog.h"
// queryable-record <=> buffer <=> DB files
template <typename T1 = unsigned int, typename T2 = bool>
class block
{
	std::pair<T1, T2> data;
	unsigned int freq = 0u;
public:
	const T1& series = data.first;  //series 指这一块是缓冲区的第几页
	T2& isdirty = data.second;	// 根据record的反馈而变
	// 是record文件夹下的db文件
	string tbname;
	string filename;
	// 表示这一页上有多少record
	int recordnum = -1;
	// 表示这一页上每条record的大小
	int bytes = 0;
	// 表示距离文件头的距离
	int offset = 0;
	bool ispin = false;
	block(const T1& t1, const T2& t2) :data(t1, t2){}
	void writeback()
	{
		if (filename.length() == 0)
			throw runtime_error("filename.length()==0\n");
		unsigned char* tempuc = buff[series];
		FILE* wr = fopen(filename.c_str(), "rb+");
		fseek(wr, offset*BLOCK_8k, SEEK_SET);
		fwrite(tempuc, sizeof(unsigned char), BLOCK_8k, wr);
		fclose(wr);
		isdirty = false;
	}
	void updatefreq() { freq++; }
	const unsigned int getfreq()const { return this->freq; }
	block& operator=(block& a)
	{
		data.first = a.data.first;
		data.second = a.data.second;
		freq = a.freq;
		filename = a.filename;
		offset = a.offset;
		ispin = a.ispin;
		return *this;
	}
};

extern vector<block<>> BufferManager;
#endif // !LLVMSQL_BUFFER_H
