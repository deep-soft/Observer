// Archive/UdfIn.cpp

#include "StdAfx.h"
#include "UdfIn.h"

extern "C"
{
  #include "CpuArch.h"
}

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

const int kNumPartitionsMax = 64;
const int kNumLogVolumesMax = 64;
const int kNumRecureseLevelsMax = 1 << 10;
const int kNumItemsMax = 1 << 27;
const int kNumFilesMax = 1 << 28;
const int kNumRefsMax = 1 << 28;
const UInt32 kNumExtentsMax = (UInt32)1 << 30;
const UInt64 kFileNameLengthTotalMax = (UInt64)1 << 33;
const UInt64 kInlineExtentsSizeMax = (UInt64)1 << 33;

void MY_FAST_CALL Crc16GenerateTable(void);

#define CRC16_INIT_VAL 0
#define CRC16_GET_DIGEST(crc) (crc)
#define CRC16_UPDATE_BYTE(crc, b) (g_Crc16Table[(((crc) >> 8) ^ (b)) & 0xFF] ^ ((crc) << 8))

#define kCrc16Poly 0x1021
UInt16 g_Crc16Table[256];

void MY_FAST_CALL Crc16GenerateTable(void)
{
  UInt32 i;
  for (i = 0; i < 256; i++)
  {
    UInt32 r = (i << 8);
    for (int j = 8; j > 0; j--)
      r = ((r & 0x8000) ? ((r << 1) ^ kCrc16Poly) : (r << 1)) & 0xFFFF;
    g_Crc16Table[i] = (UInt16)r;
  }
}

UInt16 MY_FAST_CALL Crc16_Update(UInt16 v, const void *data, size_t size)
{
  const Byte *p = (const Byte *)data;
  for (; size > 0 ; size--, p++)
    v = CRC16_UPDATE_BYTE(v, *p);
  return v;
}

UInt16 MY_FAST_CALL Crc16Calc(const void *data, size_t size)
{
  return Crc16_Update(CRC16_INIT_VAL, data, size);
}

struct CCrc16TableInit { CCrc16TableInit() { Crc16GenerateTable(); } } g_Crc16TableInit;

void CDString128::Parse(const Byte *buf) { memcpy(Data, buf, sizeof(Data)); }

void CDString::Parse(const Byte *p, unsigned size)
{
  Data.SetCapacity(size);
  memcpy(Data, p, size);
}

static UString ParseDString(const Byte *data, int size)
{
  UString res;
  wchar_t *p;
  if (size > 0)
  {
    Byte type = data[0];
    if (type == 8)
    {
      p = res.GetBuffer((int)size + 1);
      for (int i = 1; i < size; i++)
      {
        wchar_t c = data[i];
        if (c == 0)
          break;
        *p++ = c;
      }
    }
    else if (type == 16)
    {
      p = res.GetBuffer((int)size / 2 + 1);
      for (int i = 1; i + 2 <= size; i += 2)
      {
        wchar_t c = ((wchar_t)data[i] << 8) | data[i + 1];
        if (c == 0)
          break;
        *p++ = c;
      }
    }
    else
      return L"[unknow]";
    *p++ = 0;
    res.ReleaseBuffer();
  }
  return res;
}

UString CDString::   GetString() const { return ParseDString(Data, (int)Data.GetCapacity()); }
UString CDString128::GetString() const
{
  int size = Data[sizeof(Data) - 1];
  return ParseDString(Data, MyMin(size, (int)(sizeof(Data) - 1)));
}

void CTime::Parse(const Byte *buf) { memcpy(Data, buf, sizeof(Data)); }

void CRegId::Parse(const Byte *buf)
{
  Flags = buf[0];
  memcpy(Id, buf + 1, sizeof(Id));
  memcpy(Suffix, buf + 24, sizeof(Suffix));
}

// ECMA 3/7.1

struct CExtent
{
  UInt32 Len;
  UInt32 Pos;

  void Parse(const Byte *buf);
};

void CExtent::Parse(const Byte *buf)
{
  Len = Get32(buf);
  Pos = Get32(buf + 4);
}

// ECMA 3/7.2

struct CTag
{
  UInt16 Id;
  UInt16 Version;
  // Byte Checksum;
  // UInt16 SerialNumber;
  // UInt16 Crc;
  // UInt16 CrcLen;
  // UInt32 TagLocation;
  
  HRESULT Parse(const Byte *buf, size_t size);
};

HRESULT CTag::Parse(const Byte *buf, size_t size)
{
  if (size < 16)
    return S_FALSE;
  Byte sum = 0;
  int i;
  for (i = 0; i <  4; i++) sum = sum + buf[i];
  for (i = 5; i < 16; i++) sum = sum + buf[i];
  if (sum != buf[4] || buf[5] != 0) return S_FALSE;

  Id = Get16(buf);
  Version = Get16(buf + 2);
  // SerialNumber = Get16(buf + 6);
  UInt16 crc = Get16(buf + 8);
  UInt16 crcLen = Get16(buf + 10);
  // TagLocation = Get32(buf + 12);

  if (size >= 16 + (size_t)crcLen)
    if (crc == Crc16Calc(buf + 16, crcLen))
      return S_OK;
  return S_FALSE;
}

// ECMA 3/7.2.1

enum EDescriptorType
{
  DESC_TYPE_SpoaringTable = 0, // UDF
  DESC_TYPE_PrimVol = 1,
  DESC_TYPE_AnchorVolPtr = 2,
  DESC_TYPE_VolPtr = 3,
  DESC_TYPE_ImplUseVol = 4,
  DESC_TYPE_Partition = 5,
  DESC_TYPE_LogicalVol = 6,
  DESC_TYPE_UnallocSpace = 7,
  DESC_TYPE_Terminating = 8,
  DESC_TYPE_LogicalVolIntegrity = 9,
  DESC_TYPE_FileSet = 256,
  DESC_TYPE_FileId  = 257,
  DESC_TYPE_AllocationExtent = 258,
  DESC_TYPE_Indirect = 259,
  DESC_TYPE_Terminal = 260,
  DESC_TYPE_File = 261,
  DESC_TYPE_ExtendedAttrHeader = 262,
  DESC_TYPE_UnallocatedSpace = 263,
  DESC_TYPE_SpaceBitmap = 264,
  DESC_TYPE_PartitionIntegrity = 265,
  DESC_TYPE_ExtendedFile = 266,
};


void CLogBlockAddr::Parse(const Byte *buf)
{
  Pos = Get32(buf);
  PartitionRef = Get16(buf + 4);
}

void CShortAllocDesc::Parse(const Byte *buf)
{
  Len = Get32(buf);
  Pos = Get32(buf + 4);
}

/*
void CADImpUse::Parse(const Byte *buf)
{
  Flags = Get16(buf);
  UdfUniqueId = Get32(buf + 2);
}
*/

void CLongAllocDesc::Parse(const Byte *buf)
{
  Len = Get32(buf);
  Location.Parse(buf + 4);
  // memcpy(ImplUse, buf + 10, sizeof(ImplUse));
  // adImpUse.Parse(ImplUse);
}

void CEntityIdentifier::Parse(const Byte *p)
{
	Flags = p[0];
	memcpy(Identifier, p + 1, 23);
	memcpy(IdentifierSuffix, p + 24, 8);
}

bool CUdfArchive::CheckExtent(int volIndex, int partitionRef, UInt32 blockPos, UInt32 len) const
{
  const CLogVol &vol = LogVols[volIndex];
  const CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];
  UInt64 offset = ((UInt64)partition.Pos << SecLogSize) + (UInt64)blockPos * vol.BlockSize;
  return (offset + len) <= (((UInt64)partition.Pos + partition.Len) << SecLogSize);
}

bool CUdfArchive::CheckItemExtents(int volIndex, const CItem &item) const
{
  for (int i = 0; i < item.Extents.Size(); i++)
  {
    const CMyExtent &e = item.Extents[i];
    if (!CheckExtent(volIndex, e.PartitionRef, e.Pos, e.GetLen()))
      return false;
  }
  return true;
}

HRESULT CUdfArchive::ReadRaw(int volIndex, int partitionRef, UInt32 blockPos, UInt32 len, Byte *buf)
{
  if (!CheckExtent(volIndex, partitionRef, blockPos, len))
    return S_FALSE;
  const CLogVol &vol = LogVols[volIndex];
  const CPartition &partition = Partitions[vol.PartitionMaps[partitionRef].PartitionIndex];
  if (!SeekStream(_file, ((UInt64)partition.Pos << SecLogSize) + (UInt64)blockPos * vol.BlockSize, FILE_BEGIN, NULL))
	  return S_FALSE;
  return ReadStream_FALSE(_file, buf, len) ? S_OK : S_FALSE;
}

HRESULT CUdfArchive::Read(int volIndex, int partitionRef, UInt32 blockPos, UInt32 len, Byte *buf)
{
	if (!CheckExtent(volIndex, partitionRef, blockPos, len))
		return S_FALSE;
	const CLogVol &vol = LogVols[volIndex];
	CLogicalPartition *logPart = vol.LogicPartitions[partitionRef];
	if (!logPart->Seek((UInt64)blockPos * vol.BlockSize, FILE_BEGIN, NULL))
		return S_FALSE;
	return logPart->ReadData(buf, len) ? S_OK : S_FALSE;
}

HRESULT CUdfArchive::Read(int volIndex, const CLongAllocDesc &lad, Byte *buf)
{
  return Read(volIndex, lad.Location.PartitionRef, lad.Location.Pos, lad.GetLen(), (Byte *)buf);
}

HRESULT CUdfArchive::ReadFromFile(int volIndex, const CItem &item, CByteBuffer &buf)
{
  if (item.Size >= (UInt32)1 << 30)
    return S_FALSE;
  if (item.IsInline)
  {
    buf = item.InlineData;
    return S_OK;
  }
  buf.SetCapacity((size_t)item.Size);
  size_t pos = 0;
  for (int i = 0; i < item.Extents.Size(); i++)
  {
    const CMyExtent &e = item.Extents[i];
    UInt32 len = e.GetLen();
    RINOK(Read(volIndex, e.PartitionRef, e.Pos, len, (Byte *)buf + pos));
    pos += len;
  }
  return S_OK;
}


void CIcbTag::Parse(const Byte *p)
{
  // PriorDirectNum = Get32(p);
  // StrategyType = Get16(p + 4);
  // StrategyParam = Get16(p + 6);
  // MaxNumOfEntries = Get16(p + 8);
  FileType = p[11];
  // ParentIcb.Parse(p + 12);
  Flags = Get16(p + 18);
}

void CItem::Parse(const Byte *p)
{
  // Uid = Get32(p + 36);
  // Gid = Get32(p + 40);
  // Permissions = Get32(p + 44);
  // FileLinkCount = Get16(p + 48);
  // RecordFormat = p[50];
  // RecordDisplayAttr = p[51];
  // RecordLen = Get32(p + 52);
  Size = Get64(p + 56);
  NumLogBlockRecorded = Get64(p + 64);
  ATime.Parse(p + 72);
  MTime.Parse(p + 84);
  // AttrtTime.Parse(p + 96);
  // CheckPoint = Get32(p + 108);
  // ExtendedAttrIcb.Parse(p + 112);
  // ImplId.Parse(p + 128);
  // UniqueId = Get64(p + 160);
  
  CreateTime.Parse(p + 84); // Same as MTime
}

// Support for Extended File Entry
void CItem::ParseExt(const Byte *p)
{
	Size = Get64(p + 56);
	NumLogBlockRecorded = Get64(p + 72);
	ATime.Parse(p + 80);
	MTime.Parse(p + 92);
	CreateTime.Parse(p + 104);
}

// 4/14.4
struct CFileId
{
  // UInt16 FileVersion;
  Byte FileCharacteristics;
  // CByteBuffer ImplUse;
  CDString Id;
  CLongAllocDesc Icb;

  bool IsItLinkParent() const { return (FileCharacteristics & FILEID_CHARACS_Parent) != 0; }
  bool IsHidden() const { return (FileCharacteristics & FILEID_CHARACS_Hidden) != 0; }
  HRESULT Parse(const Byte *p, size_t size, size_t &processed);
};

HRESULT CFileId::Parse(const Byte *p, size_t size, size_t &processed)
{
  processed = 0;
  if (size < 38)
    return S_FALSE;
  CTag tag;
  RINOK(tag.Parse(p, size));
  if (tag.Id != DESC_TYPE_FileId)
    return S_FALSE;
  // FileVersion = Get16(p + 16);
  FileCharacteristics = p[18];
  unsigned idLen = p[19];
  Icb.Parse(p + 20);
  unsigned impLen = Get16(p + 36);
  if (size < 38 + idLen + impLen)
    return S_FALSE;
  // ImplUse.SetCapacity(impLen);
  processed = 38;
  // memcpy(ImplUse, p + processed, impLen);
  processed += impLen;
  Id.Parse(p + processed, idLen);
  processed += idLen;
  for (;(processed & 3) != 0; processed++)
    if (p[processed] != 0)
      return S_FALSE;
  return (processed <= size) ? S_OK : S_FALSE;
}

HRESULT CUdfArchive::ReadFileItem(int volIndex, const CLongAllocDesc &lad, int numRecurseAllowed)
{
  if (_progress && (Files.Size() % 100 == 0))
    RINOK(_progress->SetCompleted(Files.Size(), _processedProgressBytes));
  if (numRecurseAllowed-- == 0)
    return S_FALSE;
  CFile &file = Files.Back();
  const CLogVol &vol = LogVols[volIndex];
  CPartition &partition = Partitions[vol.PartitionMaps[lad.Location.PartitionRef].PartitionIndex];

  UInt32 key = lad.Location.Pos;
  UInt32 value;
  const UInt32 kRecursedErrorValue = (UInt32)(Int32)-1;
  if (partition.Map.Find(key, value))
  {
    if (value == kRecursedErrorValue)
      return S_FALSE;
    file.ItemIndex = value;
  }
  else
  {
    value = Items.Size();
    file.ItemIndex = (int)value;
    if (partition.Map.Set(key, kRecursedErrorValue))
      return S_FALSE;
    RINOK(ReadItem(volIndex, lad, numRecurseAllowed));
    if (!partition.Map.Set(key, value))
      return S_FALSE;
  }
  return S_OK;
}

HRESULT CUdfArchive::ReadItem(int volIndex, const CLongAllocDesc &lad, int numRecurseAllowed)
{
  if (Items.Size() > kNumItemsMax)
    return S_FALSE;
  Items.Add(CItem());
  CItem &item = Items.Back();
  
  const CLogVol &vol = LogVols[volIndex];
 
  if (lad.GetLen() != vol.BlockSize)
    return S_FALSE;

  CByteBuffer buf;
  size_t size = lad.GetLen();
  buf.SetCapacity(size);
  RINOK(Read(volIndex, lad, buf));

  const Byte *p = buf;
  RINOK(ParseFileBuf(volIndex, lad.Location.PartitionRef, p, size, item));

  if (item.IcbTag.IsDir())
  {
    if (!item.CheckChunkSizes() || !CheckItemExtents(volIndex, item))
      return S_FALSE;
    CByteBuffer buf;
    RINOK(ReadFromFile(volIndex, item, buf));
    item.Size = 0;
    item.Extents.ClearAndFree();
    item.InlineData.Free();

    const Byte *p = buf;
    size = buf.GetCapacity();
    size_t processedTotal = 0;
    for (; processedTotal < size;)
    {
      size_t processedCur;
      CFileId fileId;
      RINOK(fileId.Parse(p + processedTotal, size - processedTotal, processedCur));
      if (!fileId.IsItLinkParent())
      {
        CFile file;
        // file.FileVersion = fileId.FileVersion;
        // file.FileCharacteristics = fileId.FileCharacteristics;
        // file.ImplUse = fileId.ImplUse;
        file.Id = fileId.Id;
		file.IsHidden = fileId.IsHidden();
        
        _fileNameLengthTotal += file.Id.Data.GetCapacity();
        if (_fileNameLengthTotal > kFileNameLengthTotalMax)
          return S_FALSE;
        
        item.SubFiles.Add(Files.Size());
        if (Files.Size() > kNumFilesMax)
          return S_FALSE;
        Files.Add(file);
        RINOK(ReadFileItem(volIndex, fileId.Icb, numRecurseAllowed));
      }
      processedTotal += processedCur;
    }
  }
  else
  {
    if ((UInt32)item.Extents.Size() > kNumExtentsMax - _numExtents)
      return S_FALSE;
    _numExtents += item.Extents.Size();

    if (item.InlineData.GetCapacity() > kInlineExtentsSizeMax - _inlineExtentsSize)
      return S_FALSE;
    _inlineExtentsSize += item.InlineData.GetCapacity();
  }

  return S_OK;
}

HRESULT CUdfArchive::ParseFileBuf( int volIndex, int partitionRef, const Byte *p, size_t size, CItem &item )
{
  CTag tag;
  RINOK(tag.Parse(p, size));
  if (tag.Id != DESC_TYPE_File && tag.Id != DESC_TYPE_ExtendedFile)
    return S_FALSE;

  item.IcbTag.Parse(p + 16);
  if (item.IcbTag.FileType != ICB_FILE_TYPE_DIR &&
      item.IcbTag.FileType != ICB_FILE_TYPE_FILE &&
	  item.IcbTag.FileType != ICB_FILE_TYPE_METADATA)
    return S_FALSE;

  UInt32 extendedAttrLen;
  UInt32 allocDescriptorsLen;
  int pos;

  const CLogVol &vol = LogVols[volIndex];

  if (tag.Id == DESC_TYPE_File)
  {
	  item.Parse(p);
	  _processedProgressBytes += (UInt64)item.NumLogBlockRecorded * vol.BlockSize + size;

	  extendedAttrLen = Get32(p + 168);
	  allocDescriptorsLen = Get32(p + 172);

	  if ((extendedAttrLen & 3) != 0)
		return S_FALSE;
	  pos = 176;
	  if (extendedAttrLen > size - pos)
		return S_FALSE;
  }
  else
  {
	  item.ParseExt(p);
	  _processedProgressBytes += (UInt64)item.NumLogBlockRecorded * vol.BlockSize + size;
	  
	  extendedAttrLen = Get32(p + 208);
	  allocDescriptorsLen = Get32(p + 212);

	  if ((extendedAttrLen & 3) != 0)
		  return S_FALSE;
	  pos = 216;
	  if (extendedAttrLen > size - pos)
		  return S_FALSE;
  }
  /*
  if (extendedAttrLen != 16)
  {
    if (extendedAttrLen < 24)
      return S_FALSE;
    CTag attrTag;
    RINOK(attrTag.Parse(p + pos, size));
    if (attrTag.Id != DESC_TYPE_ExtendedAttrHeader)
      return S_FALSE;
    // UInt32 implAttrLocation = Get32(p + pos + 16);
    // UInt32 applicationlAttrLocation = Get32(p + pos + 20);
  }
  */
  pos += extendedAttrLen;

  int desctType = item.IcbTag.GetDescriptorType();
  if (allocDescriptorsLen > size - pos)
    return S_FALSE;
  if (desctType == ICB_DESC_TYPE_INLINE)
  {
    item.IsInline = true;
    item.InlineData.SetCapacity(allocDescriptorsLen);
    memcpy(item.InlineData, p + pos, allocDescriptorsLen);
  }
  else
  {
    item.IsInline = false;
    if (desctType != ICB_DESC_TYPE_SHORT && desctType != ICB_DESC_TYPE_LONG)
      return S_FALSE;
    for (UInt32 i = 0; i < allocDescriptorsLen;)
    {
      CMyExtent e;
      if (desctType == ICB_DESC_TYPE_SHORT)
      {
        if (i + 8 > allocDescriptorsLen)
          return S_FALSE;
        CShortAllocDesc sad;
        sad.Parse(p + pos + i);
        e.Pos = sad.Pos;
        e.Len = sad.Len;
        e.PartitionRef = partitionRef;
        i += 8;
      }
      else
      {
        if (i + 16 > allocDescriptorsLen)
          return S_FALSE;
        CLongAllocDesc ladNew;
        ladNew.Parse(p + pos + i);
        e.Pos = ladNew.Location.Pos;
        e.PartitionRef = ladNew.Location.PartitionRef;
        e.Len = ladNew.Len;
        i += 16;
      }
      item.Extents.Add(e);
    }
  }
	
	return S_OK;
}

HRESULT CUdfArchive::FillRefs(CFileSet &fs, int fileIndex, int parent, int numRecurseAllowed)
{
  if (_progress && (_numRefs % 10000 == 0))
  {
    RINOK(_progress->SetCompleted());
  }
  if (numRecurseAllowed-- == 0)
    return S_FALSE;
  if (_numRefs >= kNumRefsMax)
    return S_FALSE;
  _numRefs++;
  CRef ref;
  ref.FileIndex = fileIndex;
  ref.Parent = parent;
  parent = fs.Refs.Size();
  fs.Refs.Add(ref);
  const CItem &item = Items[Files[fileIndex].ItemIndex];
  for (int i = 0; i < item.SubFiles.Size(); i++)
  {
    RINOK(FillRefs(fs, item.SubFiles[i], parent, numRecurseAllowed));
  }
  return S_OK;
}

HRESULT CUdfArchive::Open2()
{
  Clear();

  UInt64 fileSize = StreamSize(_file);

  // Some UDFs contain additional 2 KB of zeros, so we also check 12, corrected to 11.
  const int kSecLogSizeMax = 12;
  Byte buf[1 << kSecLogSizeMax];
  Byte kSizesLog[] = { 11, 8, 12 };

  for (int i = 0;; i++)
  {
    if (i == sizeof(kSizesLog) / sizeof(kSizesLog[0]))
	{
		SecLogSize = 0;
		break;
	}
    SecLogSize = kSizesLog[i];
    Int32 bufSize = 1 << SecLogSize;
    if (bufSize > fileSize)
      return S_FALSE;
    if (!SeekStream(_file, -bufSize, FILE_END, NULL))
		return S_FALSE;
    if (!ReadStream_FALSE(_file, buf, bufSize))
		return S_FALSE;
    CTag tag;
    if (tag.Parse(buf, bufSize) == S_OK)
      if (tag.Id == DESC_TYPE_AnchorVolPtr)
        break;
  }
  if (SecLogSize == 12)
    SecLogSize = 11;

	// Try alternative search for anchor volume
	if (SecLogSize == 0)
	{
		SecLogSize = 11;
		Int32 bufSize = 1 << SecLogSize;

		if (!SeekStream(_file, 256 * bufSize, FILE_BEGIN, NULL))
			return S_FALSE;
		if (!ReadStream_FALSE(_file, buf, bufSize))
			return S_FALSE;

		CTag tag;
		if (tag.Parse(buf, bufSize) != S_OK)
			return S_FALSE;
		if (tag.Id != DESC_TYPE_AnchorVolPtr)
			return S_FALSE;
	}
	
  CExtent extentVDS;
  extentVDS.Parse(buf + 16);

  for (UInt32 location = extentVDS.Pos; ; location++)
  {
    size_t bufSize = 1 << SecLogSize;
    size_t pos = 0;
    if (!SeekStream(_file, (UInt64)location << SecLogSize, FILE_BEGIN, NULL))
		return S_FALSE;
    if (!ReadStream_FALSE(_file, buf, bufSize))
		return S_FALSE;
    CTag tag;
    RINOK(tag.Parse(buf + pos, bufSize - pos));
    if (tag.Id == DESC_TYPE_Terminating)
      break;
    if (tag.Id == DESC_TYPE_Partition)
    {
      if (Partitions.Size() >= kNumPartitionsMax)
        return S_FALSE;
      CPartition partition;
      // UInt32 volDescSeqNumer = Get32(buf + 16);
      // partition.Flags = Get16(buf + 20);
      partition.Number = Get16(buf + 22);
      // partition.ContentsId.Parse(buf + 24);
      
      // memcpy(partition.ContentsUse, buf + 56, sizeof(partition.ContentsUse));
      // ContentsUse is Partition Header Description.

      // partition.AccessType = Get32(buf + 184);
      partition.Pos = Get32(buf + 188);
      partition.Len = Get32(buf + 192);
      // partition.ImplId.Parse(buf + 196);
      // memcpy(partition.ImplUse, buf + 228, sizeof(partition.ImplUse));

      Partitions.Add(partition);
    }
    else if (tag.Id == DESC_TYPE_LogicalVol)
    {
      if (LogVols.Size() >= kNumLogVolumesMax)
        return S_FALSE;
      CLogVol vol;
      vol.Id.Parse(buf + 84);
      vol.BlockSize = Get32(buf + 212);
      vol.DomainId.Parse(buf + 216);

      if (vol.BlockSize < 512 || vol.BlockSize > ((UInt32)1 << 30))
        return S_FALSE;

	  if (strcmp(vol.DomainId.Id, "*OSTA UDF Compliant") != 0)
		  return S_FALSE;
      
      // memcpy(vol.ContentsUse, buf + 248, sizeof(vol.ContentsUse));
      vol.FileSetLocation.Parse(buf + 248);

      // UInt32 mapTableLength = Get32(buf + 264);
      UInt32 numPartitionMaps = Get32(buf + 268);
      if (numPartitionMaps > kNumPartitionsMax)
        return S_FALSE;
      // vol.ImplId.Parse(buf + 272);
      // memcpy(vol.ImplUse, buf + 128, sizeof(vol.ImplUse));
      size_t pos = 440;
      for (UInt32 i = 0; i < numPartitionMaps; i++)
      {
        if (pos + 2 > bufSize)
          return S_FALSE;
        CPartitionMap pm;
        pm.Type = buf[pos];
        // pm.Length = buf[pos + 1];
        Byte len = buf[pos + 1];

        if (pos + len > bufSize)
          return S_FALSE;
        
        // memcpy(pm.Data, buf + pos + 2, pm.Length - 2);
        if (pm.Type == 1)
        {
          // pm.VolSeqNumber = Get16(buf + pos + 2);
          pm.PartitionNumber = Get16(buf + pos + 4);
        }
		else if (pm.Type == 2)
		{
			pm.PartitionTypeIdentifier.Parse(buf + pos + 4);
			// pm.VolSeqNumber = Get16(buf + pos + 36);
			pm.PartitionNumber = Get16(buf + pos + 38);
		}
        else
          return S_FALSE;
        pos += len;
        vol.PartitionMaps.Add(pm);
      }
      LogVols.Add(vol);
    }
  }

  UInt64 totalSize = 0;

  int volIndex;
  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
    CLogVol &vol = LogVols[volIndex];
    for (int pmIndex = 0; pmIndex < vol.PartitionMaps.Size(); pmIndex++)
    {
      CPartitionMap &pm = vol.PartitionMaps[pmIndex];
      int i;
      for (i = 0; i < Partitions.Size(); i++)
      {
        CPartition &part = Partitions[i];
        if (part.Number == pm.PartitionNumber)
        {
          if ((part.VolIndex >= 0) && (pm.Type == 1))
            return S_FALSE;
          pm.PartitionIndex = i;
          part.VolIndex = volIndex;

          if (pm.Type == 1)
			totalSize += (UInt64)part.Len << SecLogSize;
          break;
        }
      }
      if (i == Partitions.Size())
        return S_FALSE;
    }
  }

  if (_progress)
	RINOK(_progress->SetTotal(totalSize));

  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
	  CLogVol &vol = LogVols[volIndex];
	  for (int partIndex = 0; partIndex < vol.PartitionMaps.Size(); partIndex++)
	  {
		  CPartitionMap &partMap = vol.PartitionMaps[partIndex];

		  if (partMap.Type == 1)
		  {
			  const CPartition &partition = Partitions[partMap.PartitionIndex];
			  CType1Partition *t1part = new CType1Partition(_file, partition, SecLogSize);
			  vol.LogicPartitions.Add(t1part);
		  }
		  else if (partMap.Type == 2)
		  {
			  CLongAllocDesc metaFileExtent = vol.FileSetLocation;
			  if (metaFileExtent.GetLen() < 512)
				  return S_FALSE;

			  CByteBuffer metaFileBuf;
			  size_t mfbSize = metaFileExtent.GetLen();
			  metaFileBuf.SetCapacity(mfbSize);

			  RINOK(ReadRaw(volIndex, metaFileExtent.Location.PartitionRef, metaFileExtent.Location.Pos, (UInt32) mfbSize, metaFileBuf));
			  
			  const Byte *p = metaFileBuf;
			  CTag tag;
			  RINOK(tag.Parse(p, mfbSize));
			  if (tag.Id != DESC_TYPE_ExtendedFile)
				  return S_FALSE;

			  CItem metaFileItem;
			  RINOK(ParseFileBuf(volIndex, partMap.PartitionIndex, p, mfbSize, metaFileItem));

			  CByteBuffer metaFileContent;
			  metaFileContent.SetCapacity((size_t) metaFileItem.Size);
			  RINOK(ReadFromFile(volIndex, metaFileItem, metaFileContent));
			  
			  CMetadataPartition *mdpart = new CMetadataPartition(metaFileContent, vol);
			  vol.LogicPartitions.Add(mdpart);
		  }
		  else
			  return S_FALSE;
	  }
  }

  for (volIndex = 0; volIndex < LogVols.Size(); volIndex++)
  {
    CLogVol &vol = LogVols[volIndex];

    CLongAllocDesc nextExtent = vol.FileSetLocation;
    // while (nextExtent.ExtentLen != 0)
    // for (int i = 0; i < 1; i++)
    {
      if (nextExtent.GetLen() < 512)
        return S_FALSE;
      CByteBuffer buf;
      buf.SetCapacity(nextExtent.GetLen());
      RINOK(Read(volIndex, nextExtent, buf));
      const Byte *p = buf;
      size_t size = nextExtent.GetLen();

      CTag tag;
      RINOK(tag.Parse(p, size));
      if (tag.Id != DESC_TYPE_FileSet)
        return S_FALSE;
      
      CFileSet fs;
      fs.RecodringTime.Parse(p + 16);
      // fs.InterchangeLevel = Get16(p + 18);
      // fs.MaxInterchangeLevel = Get16(p + 20);
      // fs.FileSetNumber = Get32(p + 40);
      // fs.FileSetDescNumber = Get32(p + 44);
      
      // fs.Id.Parse(p + 304);
      // fs.CopyrightId.Parse(p + 336);
      // fs.AbstractId.Parse(p + 368);
      
      fs.RootDirICB.Parse(p + 400);
      // fs.DomainId.Parse(p + 416);
      
      // fs.SystemStreamDirICB.Parse(p + 464);
      
      vol.FileSets.Add(fs);

      // nextExtent.Parse(p + 448);
    }

    for (int fsIndex = 0; fsIndex < vol.FileSets.Size(); fsIndex++)
    {
      CFileSet &fs = vol.FileSets[fsIndex];
      int fileIndex = Files.Size();
      Files.Add(CFile());
      RINOK(ReadFileItem(volIndex, fs.RootDirICB, kNumRecureseLevelsMax));
      RINOK(FillRefs(fs, fileIndex, -1, kNumRecureseLevelsMax));
    }
  }

  return S_OK;
}

bool CUdfArchive::Open(const wchar_t *path, CProgressVirt *progress)
{
  _progress = progress;
  _file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (_file == INVALID_HANDLE_VALUE)
	  return false;

  HRESULT res;
  try
  {
	  res = Open2();
  }
  catch(...)
  {
	  Clear();
	  res = S_FALSE;
  }

  return (res == S_OK);
}

void CUdfArchive::Close()
{
	Clear();
	if (_file != INVALID_HANDLE_VALUE)
		CloseHandle(_file);
}

void CUdfArchive::Clear()
{
  Partitions.Clear();
  LogVols.Clear();
  Items.Clear();
  Files.Clear();
  _fileNameLengthTotal = 0;
  _numRefs = 0;
  _numExtents = 0;
  _inlineExtentsSize = 0;
  _processedProgressBytes = 0;
}

UString CUdfArchive::GetComment() const
{
  UString res;
  for (int i = 0; i < LogVols.Size(); i++)
  {
    if (i > 0)
      res += L" ";
    res += LogVols[i].GetName();
  }
  return res;
}

static UString GetSpecName(const UString &name)
{
  UString name2 = name;
  name2.Trim();
  if (name2.IsEmpty())
  {
    /*
    wchar_t s[32];
    ConvertUInt64ToString(id, s);
    return L"[" + (UString)s + L"]";
    */
    return L"[]";
  }
  return name;
}

static void UpdateWithName(UString &res, const UString &addString)
{
  if (res.IsEmpty())
    res = addString;
  else
    res = addString + WCHAR_PATH_SEPARATOR + res;
}

UString CUdfArchive::GetItemPath(int volIndex, int fsIndex, int refIndex,
    bool showVolName, bool showFsName) const
{
  // showVolName = true;
  const CLogVol &vol = LogVols[volIndex];
  const CFileSet &fs = vol.FileSets[fsIndex];

  UString name;

  for (;;)
  {
    const CRef &ref = fs.Refs[refIndex];
    refIndex = ref.Parent;
    if (refIndex < 0)
      break;
    UpdateWithName(name, GetSpecName(Files[ref.FileIndex].GetName()));
  }

  if (showFsName)
  {
    wchar_t s[32];
    ConvertUInt64ToString(fsIndex, s);
    UString newName = L"File Set ";
    newName += s;
    UpdateWithName(name, newName);
  }

  if (showVolName)
  {
    wchar_t s[32];
    ConvertUInt64ToString(volIndex, s);
    UString newName = s;
    UString newName2 = vol.GetName();
    if (newName2.IsEmpty())
      newName2 = L"Volume";
    newName += L'-';
    newName += newName2;
    UpdateWithName(name, newName);
  }
  return name;
}

CUdfArchive::CUdfArchive()
{
	_file = INVALID_HANDLE_VALUE;
}

CUdfArchive::~CUdfArchive()
{
	Close();
}

int CUdfArchive::GetSubItemByName( const wchar_t* name, int parentIndex )
{
	if (!name || !*name)
		return parentIndex;
	
	CItem parent = Items[parentIndex];
	for (int i = 0; i < parent.SubFiles.Size(); i++)
	{
		int subIndex = parent.SubFiles[i];
		UString uname = this->Files[subIndex].GetName();
		if (wcscmp(name, uname) == 0)
			return subIndex;
	}
	
	return -1;
}

int CUdfArchive::DumpFileContent(const CFile& fileObj, const wchar_t* destPath, const ExtractProcessCallbacks* epc)
{
	HANDLE context = NULL;

	int result = SER_SUCCESS;
	const CItem &itemObj = Items[fileObj.ItemIndex];

	DWORD nOutFileAttr = fileObj.IsHidden ? FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE : FILE_ATTRIBUTE_NORMAL;
	HANDLE hFile = CreateFileW(destPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, nOutFileAttr, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		result = SER_ERROR_WRITE;
	}
	else
	{
		if (itemObj.IsInline)
		{
			DWORD dwWritten;
			size_t nNumWrite = itemObj.InlineData.GetCapacity();
			if (!WriteFile(hFile, (const Byte *) itemObj.InlineData, (DWORD) nNumWrite, &dwWritten, NULL))
				result = SER_ERROR_WRITE;
		}
		else
		{
			size_t nBufSize = 64 * 1024;
			char* buf = (char*) malloc(nBufSize);

			DWORD dwBytesRead, dwBytesWritten;
			for (int i = 0; i < itemObj.Extents.Size(); i++)
			{
				const CMyExtent& extent = itemObj.Extents[i];
				const CPartition& part = Partitions[extent.PartitionRef];
				const CLogVol& vol = LogVols[part.VolIndex];

				UInt32 bytesLeft = extent.Len;
				DWORD copySize;

				LARGE_INTEGER movePos;
				movePos.QuadPart = ((__int64)part.Pos << SecLogSize) + ((__int64)extent.Pos * vol.BlockSize);

				SetFilePointer(_file, movePos.LowPart, &movePos.HighPart, FILE_BEGIN);
				while (bytesLeft > 0)
				{
					copySize = (bytesLeft > nBufSize) ? (DWORD)nBufSize : bytesLeft;

					if (!ReadFile(_file, buf , copySize, &dwBytesRead, NULL))
					{
						result = SER_ERROR_READ;
						break;
					}

					if (!WriteFile(hFile, buf, dwBytesRead, &dwBytesWritten, NULL))
					{
						result = SER_ERROR_WRITE;
						break;
					}

					bytesLeft -= copySize;

					// Report extraction progress
					if (itemObj.Size && epc && epc->FileProgress)
					{
						if (!epc->FileProgress(epc->signalContext, copySize))
						{
							result = SER_USERABORT;
							break;
						}
					}
				} //while

				if (result != SER_SUCCESS) break;
			}  //for
			free(buf);
		} // if inline

		CloseHandle(hFile);

		if (result != SER_SUCCESS)
			DeleteFileW(destPath);
	}

	return result;
}

FILETIME CUdfArchive::GetCreatedTime() const
{
	FILETIME res = {0};
	if (LogVols.Size() == 1)
	{
		const CLogVol &vol = LogVols[0];
		if (vol.FileSets.Size() >= 1)
			vol.FileSets[0].RecodringTime.GetFileTime(res);
	}
	return res;
}

//////////////////////////////////////////////////////////////////////////

bool CType1Partition::ReadData( Byte* buf, size_t size )
{
	return ReadStream_FALSE(m_hFile, buf, size);
}

bool CType1Partition::Seek( UInt64 pos, int origin, UInt64 *currentPos )
{
	return SeekStream(m_hFile, m_nPartitionOffset + pos, origin, currentPos);
}

bool CMetadataPartition::ReadData( Byte* buf, size_t size )
{
	if (m_contentPos + size > m_metaFileContent.GetCapacity())
		return false;
	
	Byte* p = m_metaFileContent;
	memcpy(buf, p + m_contentPos, size);
	return true;
}

bool CMetadataPartition::Seek( UInt64 pos, int origin, UInt64 *currentPos )
{
	size_t nContentSize = m_metaFileContent.GetCapacity();
	switch (origin)
	{
		case FILE_BEGIN:
			m_contentPos = pos;
			break;
		case FILE_CURRENT:
			m_contentPos += pos;
			break;
		case FILE_END:
			m_contentPos = nContentSize + pos;
			break;
		default:
			return false;
	}

	if (m_contentPos > (UInt64) nContentSize)
		return false;

	if (currentPos)
		*currentPos = m_contentPos;
	return true;
}
