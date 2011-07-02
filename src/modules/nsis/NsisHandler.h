// NSisHandler.h

#ifndef __NSIS_HANDLER_H
#define __NSIS_HANDLER_H

#include "Common/MyCom.h"
#include "../IArchive.h"

#include "NsisIn.h"

#include "../../Common/CreateCoder.h"

namespace NArchive {
namespace NNsis {

class CHandler:
  public IInArchive,
  PUBLIC_ISetCompressCodecsInfo
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _inStream;
  CInArchive _archive;
  
  UInt64 _solidStreamPos;
  UInt64 _solidBufItemStartPos;
  CByteBuffer _solidItemBuf;

  DECL_EXTERNAL_CODECS_VARS

  bool GetUncompressedSize(int index, UInt32 &size);
  bool GetCompressedSize(int index, UInt32 &size);

  UString GetMethod(bool useItemFilter, UInt32 dictionary) const;
public:
  STDMETHODIMP SetProperty(UInt32 index, PROPID propID, PROPVARIANT *value);
public:
  MY_QUERYINTERFACE_BEGIN2(IInArchive)
  QUERY_ENTRY_ISetCompressCodecsInfo
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  INTERFACE_IInArchive(;)

  DECL_ISetCompressCodecsInfo
};

}}

#endif
