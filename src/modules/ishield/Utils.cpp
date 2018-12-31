#include "stdafx.h"
#include "Utils.h"
#include "zlib.h"

#define ror8(x,n)   (((x) >> ((int)(n))) | ((x) << (8 - (int)(n))))
#define rol8(x,n)   (((x) << ((int)(n))) | ((x) >> (8 - (int)(n))))

void CombinePath(char* buffer, size_t bufferSize, int numParts, ...)
{
	va_list argptr;
	va_start(argptr, numParts);
	for (int i = 0; i < numParts; i++)
	{
		char* next = va_arg(argptr, char*);
		if (next && *next)
		{
			if (buffer[0])
				strcat_s(buffer, bufferSize, "\\");

			strcat_s(buffer, bufferSize, next);
		}
	}
	va_end(argptr);
}

void CombinePath(wchar_t* buffer, size_t bufferSize, int numParts, ...)
{
	va_list argptr;
	va_start(argptr, numParts);
	for (int i = 0; i < numParts; i++)
	{
		wchar_t* next = va_arg(argptr, wchar_t*);
		if (next && *next)
		{
			if (buffer[0])
				wcscat_s(buffer, bufferSize, L"\\");

			wcscat_s(buffer, bufferSize, next);
		}
	}
	va_end(argptr);
}

DWORD GetMajorVersion(DWORD fileVersion)
{
	if (fileVersion >> 24 == 1)
	{
		return (fileVersion >> 12) & 0xf;
	}
	else if (fileVersion >> 24 == 2 || fileVersion >> 24 == 4)
	{
		DWORD major = (fileVersion & 0xffff);
		if (major != 0) major /= 100;
		return major;
	}

	return 0;
}

std::wstring ConvertStrings( std::string &input )
{
	int numChars = MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, NULL, 0);
	
	size_t bufSize = (numChars + 1) * sizeof(wchar_t);
	wchar_t* buf = (wchar_t*) malloc(bufSize);
	memset(buf, 0, bufSize);
	
	MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, buf, numChars + 1);
	std::wstring output(buf);
	
	free(buf);
	return output;
}

std::wstring GenerateCabPatern(const wchar_t* headerFileName)
{
	wchar_t* nameBuf = _wcsdup(headerFileName);
	
	wchar_t* dot = wcsrchr(nameBuf, L'.');
	size_t pos = dot ? dot - nameBuf : wcslen(nameBuf);
	for (pos--; isdigit(nameBuf[pos]); pos--)
	{
		nameBuf[pos] = L'\0';
	}
	nameBuf[pos+1] = L'\0';

	std::wstring result(nameBuf);
	result.append(L"%d.cab");
	
	free(nameBuf);
	return result;
}

void DecryptBuffer(BYTE* buf, DWORD bufSize, DWORD* seed)
{
	BYTE ts;
	for (; bufSize > 0; bufSize--, buf++, (*seed)++) {
		ts = *buf ^ 0xd5;
		ts = ror8(ts, 2);  //__asm { ror byte ptr ts, 2 };
		*buf = ts - (BYTE)(*seed % 0x47);
	}
}

bool UnpackBuffer( BYTE* inBuf, size_t inSize, BYTE* outBuf, size_t* outBufferSize, size_t* outDataSize, bool blockStyle )
{
	int ret;
	z_stream strm = {0};

	if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
		return false;

	strm.avail_in = (uInt) inSize;
	strm.next_in = inBuf;

	BYTE* bufPtr = outBuf;
	size_t nextOutSize = *outBufferSize;
	*outDataSize = 0;
	do 
	{
		strm.avail_out = (uInt) nextOutSize;
		strm.next_out = bufPtr;
		
		ret = inflate(&strm, blockStyle ? Z_BLOCK : Z_FINISH);
		if (ret == Z_NEED_DICT)
			ret = Z_DATA_ERROR;
		if (ret < 0) break;

		if (blockStyle && ret == Z_OK)
			ret = Z_STREAM_END;

		*outDataSize += nextOutSize - strm.avail_out;

		if (strm.avail_out == 0 && ret != Z_STREAM_END)
		{
			nextOutSize = 4 * 1024;
			bufPtr = outBuf + *outBufferSize;
			
			*outBufferSize = *outBufferSize + nextOutSize;
			outBuf = (BYTE*) realloc(outBuf, *outBufferSize);
		}

	} while (ret != Z_STREAM_END);

	inflateEnd(&strm);
	return (ret == Z_STREAM_END);
}

uint8_t* find_bytes(const uint8_t* buffer, size_t bufferSize, const uint8_t* pattern, size_t patternSize)
{
	const uint8_t *p = buffer;
	size_t buffer_left = bufferSize;
	while ((p = (const uint8_t*) memchr(p, pattern[0], buffer_left)) != NULL)
	{
		if (patternSize > buffer_left)
			break;

		if (memcmp(p, pattern, patternSize) == 0)
			return (uint8_t*)p;

		++p;
		--buffer_left;
	}

	return NULL;
}
