#ifndef ISEncSfxFile_h__
#define ISEncSfxFile_h__

#include "ISCabFile.h"

namespace ISSfx
{

#pragma pack(push, 1)

struct SfxFileHeaderA
{
	char Name[256];
	DWORD Unknown1;
	DWORD Type;
	DWORD Unknown2;
	DWORD CompressedSize;
	DWORD Unknown3[2];
	DWORD IsCompressed;
	char Unknown4[28];
};

struct SfxFileHeaderW
{
	DWORD NameBytes;
	WORD Type;
	DWORD Unknown1;
	DWORD CompressedSize;
	DWORD Unknown2[2];
	WORD IsCompressed;
};

// Meaning of the each time field is guessed, can be wrong
struct SfxFileHeaderWTimePart
{
	FILETIME CreationTime;
	FILETIME LastModificationTime;
	FILETIME LastAccessTime;
};

#pragma pack(pop)

struct SfxFileEntry
{
	char Name[MAX_PATH];
	__int64 StartOffset;
	DWORD Type;
	DWORD CompressedSize;
	bool IsCompressed;
	bool IsUnicode;
	FILETIME CreationTime;
	FILETIME LastWriteTime;
};

class ISEncSfxFile : public ISCabFile
{
protected:
	std::vector<SfxFileEntry> m_vFiles;
	
	void GenerateInfoFile() { /* Not used for this class */ }
	bool InternalOpen(CFileStream* headerFile);

	int DecodeFile(const SfxFileEntry *pEntry, CFileStream* dest, DWORD chunkSize, __int64 *unpackedSize, ExtractProcessCallbacks *pctx) const;
	int CopyPlainFile(const SfxFileEntry *pEntry, CFileStream* dest, __int64 *unpackedSize, ExtractProcessCallbacks *pctx) const;

public:
	ISEncSfxFile();
	~ISEncSfxFile();

	int GetTotalFiles() const;
	bool GetFileInfo(int itemIndex, StorageItemInfo* itemInfo) const;
	int ExtractFile(int itemIndex, CFileStream* targetFile, ExtractProcessCallbacks progressCtx);

	void Close();

	DWORD MajorVersion() const { return -1; }
	bool HasInfoData() const { return false; }
	const wchar_t* GetCompression() const { return L"ZLib"; }
};

} // namespace ISSfx

#endif // ISEncSfxFile_h__
