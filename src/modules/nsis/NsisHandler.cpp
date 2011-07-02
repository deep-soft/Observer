// NSisHandler.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "Common/ComTry.h"
#include "Common/IntToString.h"
#include "Common/NewHandler.h"

#include "Windows/PropVariant.h"

#include "../../Common/StreamUtils.h"

#include "../Common/ItemNameUtils.h"

#include "NsisHandler.h"

#define Get32(p) GetUi32(p)

using namespace NWindows;

namespace NArchive {
namespace NNsis {

static const wchar_t *kBcjMethod = L"BCJ";
static const wchar_t *kUnknownMethod = L"Unknown";

static const wchar_t *kMethods[] =
{
  L"Copy",
  L"Deflate",
  L"BZip2",
  L"LZMA"
};

static const int kNumMethods = sizeof(kMethods) / sizeof(kMethods[0]);

static STATPROPSTG kProps[] =
{
  { NULL, kpidPath, VT_BSTR},
  { NULL, kpidSize, VT_UI8},
  { NULL, kpidPackSize, VT_UI8},
  { NULL, kpidMTime, VT_FILETIME},
  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidSolid, VT_BOOL}
};

static STATPROPSTG kArcProps[] =
{
  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidSolid, VT_BOOL}
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch(propID)
  {
    case kpidMethod:
    {
      UInt32 dict = 1;
      bool filter = false;
      for (int i = 0; i < _archive.Items.Size(); i++)
      {
        const CItem &item = _archive.Items[i];
        filter |= item.UseFilter;
        if (item.DictionarySize > dict)
          dict = item.DictionarySize;
      }
      prop = GetMethod(filter, dict);
      break;
    }
    case kpidSolid: prop = _archive.IsSolid; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 * maxCheckStartPosition, IArchiveOpenCallback * /* openArchiveCallback */)
{
  COM_TRY_BEGIN
  Close();
  {
    if(_archive.Open(
        EXTERNAL_CODECS_VARS
        stream, maxCheckStartPosition) != S_OK)
      return S_FALSE;
    _inStream = stream;
	
	_solidStreamPos = 0;
	_solidBufItemStartPos = 0;
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _archive.Clear();
  _archive.Release();
  _inStream.Release();
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _archive.Items.Size()
  #ifdef NSIS_SCRIPT
    + 1
  #endif
  ;
  return S_OK;
}

static UString ConvertUInt32ToString(UInt32 value)
{
  wchar_t buffer[32];
  ConvertUInt64ToString(value, buffer);
  return buffer;
}

static UString GetStringForSizeValue(UInt32 value)
{
  for (int i = 31; i >= 0; i--)
    if ((UInt32(1) << i) == value)
      return ConvertUInt32ToString(i);
  UString result;
  if (value % (1 << 20) == 0)
  {
    result += ConvertUInt32ToString(value >> 20);
    result += L"m";
  }
  else if (value % (1 << 10) == 0)
  {
    result += ConvertUInt32ToString(value >> 10);
    result += L"k";
  }
  else
  {
    result += ConvertUInt32ToString(value);
    result += L"b";
  }
  return result;
}

UString CHandler::GetMethod(bool useItemFilter, UInt32 dictionary) const
{
  NMethodType::EEnum methodIndex = _archive.Method;
  UString method;
  if (_archive.IsSolid && _archive.UseFilter || !_archive.IsSolid && useItemFilter)
  {
    method += kBcjMethod;
    method += L" ";
  }
  method += (methodIndex < kNumMethods) ? kMethods[methodIndex] : kUnknownMethod;
  if (methodIndex == NMethodType::kLZMA)
  {
    method += L":";
    method += GetStringForSizeValue(_archive.IsSolid ? _archive.DictionarySize: dictionary);
  }
  return method;
}

bool CHandler::GetUncompressedSize(int index, UInt32 &size)
{
  size = 0;
  const CItem &item = _archive.Items[index];
  if (item.SizeIsDefined)
     size = item.Size;
  else if (_archive.IsSolid && item.EstimatedSizeIsDefined)
     size  = item.EstimatedSize;
  else
    return false;
  return true;
}

bool CHandler::GetCompressedSize(int index, UInt32 &size)
{
  size = 0;
  const CItem &item = _archive.Items[index];
  if (item.CompressedSizeIsDefined)
    size = item.CompressedSize;
  else
  {
    if (_archive.IsSolid)
    {
      if (index == 0)
        size = _archive.FirstHeader.GetDataSize();
      else
        return false;
    }
    else
    {
      if (!item.IsCompressed)
        size = item.Size;
      else
        return false;
    }
  }
  return true;
}


STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  #ifdef NSIS_SCRIPT
  if (index >= (UInt32)_archive.Items.Size())
  {
    switch(propID)
    {
      case kpidPath:  prop = L"[NSIS].nsi"; break;
      case kpidSize:
      case kpidPackSize:  prop = (UInt64)_archive.Script.Length(); break;
      case kpidSolid:  prop = false; break;
    }
  }
  else
  #endif
  {
    const CItem &item = _archive.Items[index];
    switch(propID)
    {
      case kpidPath:
      {
        UString s = NItemName::WinNameToOSName(item.GetReducedName());
        if (!s.IsEmpty())
          prop = (const wchar_t *)s;
        break;
      }
      case kpidSize:
      {
        UInt32 size;
        if (GetUncompressedSize(index, size))
          prop = (UInt64)size;
        break;
      }
      case kpidPackSize:
      {
        UInt32 size;
        if (GetCompressedSize(index, size))
          prop = (UInt64)size;
        break;
      }
      case kpidMTime:
      {
        if (item.MTime.dwHighDateTime > 0x01000000 &&
            item.MTime.dwHighDateTime < 0xFF000000)
          prop = item.MTime;
        break;
      }
      case kpidMethod:  prop = GetMethod(item.UseFilter, item.DictionarySize); break;
      case kpidSolid:  prop = _archive.IsSolid; break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::SetProperty( UInt32 index, PROPID propID, PROPVARIANT *value )
{
	COM_TRY_BEGIN
	NWindows::NCOM::CPropVariant prop;
	prop.Attach(value);
#ifdef NSIS_SCRIPT
	if (index >= (UInt32)_archive.Items.Size())
	{
		//Do nothing for script file
	}
	else
#endif
	{
		CItem &item = _archive.Items[index];
		switch(propID)
		{
		case kpidSize:
			{
				if (!item.SizeIsDefined)
				{
					item.Size = prop.uintVal;
					item.SizeIsDefined = true;
				}
				break;
			}
		}
	}
	prop.Detach(value);
	return S_OK;
	COM_TRY_END
}

STDMETHODIMP CHandler::Extract(const UInt32* indices, UInt32 numItems,
    Int32 _aTestMode, IArchiveExtractCallback *extractCallback)
{
	COM_TRY_BEGIN
	bool testMode = (_aTestMode != 0);
	bool allFilesMode = (numItems == UInt32(-1));
	if (allFilesMode)
		GetNumberOfItems(&numItems);
	if(numItems == 0)
		return S_OK;
	UInt64 totalSize = 0;
	bool fakeOnly = true;

	UInt32 i;
	for(i = 0; i < numItems; i++)
	{
		UInt32 index = (allFilesMode ? i : indices[i]);
#ifdef NSIS_SCRIPT
		if (index >= (UInt32)_archive.Items.Size())
		{
			totalSize += _archive.Script.Length();
			fakeOnly = fakeOnly && true;
		}
		else
#endif
		{
			UInt32 size;
			if (_archive.IsSolid)
			{
				GetUncompressedSize(index, size);
				UInt64 pos = _archive.GetPosOfSolidItem(index);
				if (pos > totalSize)
					totalSize = pos + size;
			}
			else
			{
				if (GetUncompressedSize(index, size))
					totalSize += size;
				else
				{
					GetCompressedSize(index, size);
					totalSize += size;
				}
			}
			fakeOnly = false;
		}
	}
	extractCallback->SetTotal(totalSize);

	UInt64 currentTotalSize = 0;
	UInt32 currentItemSize = 0;

	bool solidInit = !fakeOnly;
	if (_archive.IsSolid && !fakeOnly && _solidStreamPos > 0)
	{
		UInt64 itemStartPos = _archive.GetPosOfSolidItem(indices[0]);
		solidInit = (itemStartPos < _solidStreamPos) && (itemStartPos != _solidBufItemStartPos);
	}

	UInt64 streamPos = _archive.IsSolid ? _solidStreamPos : 0;
	if (_archive.IsSolid && solidInit)
	{
		RINOK(_inStream->Seek(_archive.StreamOffset, STREAM_SEEK_SET, NULL));
		bool useFilter;
		RINOK(_archive.Decoder.Init(
			EXTERNAL_CODECS_VARS
			_inStream, _archive.Method, _archive.FilterFlag, useFilter));
		streamPos = 0;
	}

	CByteBuffer byteBuf;
	const UInt32 kBufferLength = 1 << 16;
	byteBuf.SetCapacity(kBufferLength);
	Byte *buffer = byteBuf;

	bool dataError = false;
	for (i = 0; i < numItems; i++, currentTotalSize += currentItemSize)
	{
		currentItemSize = 0;
		RINOK(extractCallback->SetCompleted(&currentTotalSize));
		CMyComPtr<ISequentialOutStream> realOutStream;
		Int32 askMode = testMode ?
			NExtract::NAskMode::kTest :
			NExtract::NAskMode::kExtract;
		UInt32 index = allFilesMode ? i : indices[i];

		RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

#ifdef NSIS_SCRIPT
		if (index >= (UInt32)_archive.Items.Size())
		{
			currentItemSize = _archive.Script.Length();
			if (!testMode && !realOutStream)
				continue;
			RINOK(extractCallback->PrepareOperation(askMode));
			if (!testMode)
				RINOK(WriteStream(realOutStream, (const char *)_archive.Script, (UInt32)_archive.Script.Length()));
		}
		else
#endif
		{
			const CItem &item = _archive.Items[index];

			if (_archive.IsSolid)
				GetUncompressedSize(index, currentItemSize);
			else
				GetCompressedSize(index, currentItemSize);

			if (!testMode && !realOutStream)
				continue;

			RINOK(extractCallback->PrepareOperation(askMode));

			if (!dataError)
			{
				bool needDecompress = false;
				bool sizeIsKnown = false;
				UInt32 fullSize = 0;

				bool writeToTemp = false;
				bool readFromTemp = false;

				if (_archive.IsSolid)
				{
					UInt64 pos = _archive.GetPosOfSolidItem(index);
					while(streamPos < pos)
					{
						size_t processedSize = (UInt32)MyMin(pos - streamPos, (UInt64)kBufferLength);
						HRESULT res = _archive.Decoder.Read(buffer, &processedSize);
						if (res != S_OK)
						{
							if (res != S_FALSE)
								return res;
							dataError = true;
							break;
						}
						if (processedSize == 0)
						{
							dataError = true;
							break;
						}
						streamPos += processedSize;
					}
					if (streamPos == pos)
					{
						Byte buffer2[4];
						size_t processedSize = 4;
						RINOK(_archive.Decoder.Read(buffer2, &processedSize));
						if (processedSize != 4)
							return E_FAIL;
						streamPos += processedSize;
						fullSize = Get32(buffer2);
						sizeIsKnown = true;
						needDecompress = true;

						if (!testMode && (index + 1 < (UInt32) _archive.Items.Size()))
						{
							_solidItemBuf.Free();
							_solidBufItemStartPos = 0;
							UInt64 nextPos = _archive.GetPosOfSolidItem(index + 1);
							if (nextPos == pos)
							{
								_solidItemBuf.SetCapacity(fullSize);
								_solidBufItemStartPos = pos;
								writeToTemp = true;
							}
						}
					}
					else if (pos == _solidBufItemStartPos)
						readFromTemp = true;
					else
						return S_FALSE;
				}
				else
				{
					RINOK(_inStream->Seek(_archive.GetPosOfNonSolidItem(index) + 4, STREAM_SEEK_SET, NULL));
					if (item.IsCompressed)
					{
						needDecompress = true;
						bool useFilter;
						RINOK(_archive.Decoder.Init(
							EXTERNAL_CODECS_VARS
							_inStream, _archive.Method, _archive.FilterFlag, useFilter));
						// fullSize = Get32(buffer); // It's bug !!!
						// Test it: what is exact fullSize?
						fullSize =  0xFFFFFFFF;
					}
					else
						fullSize = item.Size;
				}

				if (!dataError)
				{
					if (needDecompress)
					{
						UInt64 offset = 0;
						while(!sizeIsKnown || fullSize > 0)
						{
							UInt32 curSize = kBufferLength;
							if (sizeIsKnown && curSize > fullSize)
								curSize = fullSize;
							size_t processedSize = curSize;
							HRESULT res = _archive.Decoder.Read(buffer, &processedSize);
							if (res != S_OK)
							{
								if (res != S_FALSE)
									return res;
								dataError = true;
								break;
							}
							if (processedSize == 0)
							{
								if (sizeIsKnown)
									dataError = true;
								break;
							}

							if (writeToTemp)
								memcpy((Byte *)_solidItemBuf + (size_t)offset, buffer, processedSize);

							fullSize -= (UInt32)processedSize;
							streamPos += processedSize;
							offset += processedSize;

							UInt64 completed;
							if (_archive.IsSolid)
								completed = currentTotalSize + offset;
							else
								completed = streamPos;
							RINOK(extractCallback->SetCompleted(&completed));
							if (!testMode)
								RINOK(WriteStream(realOutStream, buffer, processedSize));
						}
					}
					else
					{
						if (readFromTemp)
						{
							if (!testMode)
								RINOK(WriteStream(realOutStream, _solidItemBuf, _solidItemBuf.GetCapacity()));
						}
						else
							while(fullSize > 0)
							{
								UInt32 curSize = MyMin(fullSize, kBufferLength);
								UInt32 processedSize;
								RINOK(_inStream->Read(buffer, curSize, &processedSize));
								if (processedSize == 0)
								{
									dataError = true;
									break;
								}
								fullSize -= processedSize;
								streamPos += processedSize;
								if (!testMode)
									RINOK(WriteStream(realOutStream, buffer, processedSize));
							}
					}
				}
			}
		}
		_solidStreamPos = _archive.IsSolid ? streamPos : 0;
		realOutStream.Release();
		RINOK(extractCallback->SetOperationResult(dataError	? NExtract::NOperationResult::kDataError : NExtract::NOperationResult::kOK));
	} //for

	return S_OK;
	COM_TRY_END
}

IMPL_ISetCompressCodecsInfo

}}
