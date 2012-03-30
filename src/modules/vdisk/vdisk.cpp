// vdisk.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <vcclr.h>
#include <msclr/marshal.h>
#include "ModuleDef.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Reflection;
using namespace DiscUtils;

ref class VDFileInfo
{
public:
	DiscFileSystemInfo^ Ref;
	int VolumeIndex;

	VDFileInfo(DiscFileSystemInfo^ objRef, int volIndex)
		: Ref(objRef), VolumeIndex(volIndex)
	{
		//
	}
};

ref class AssemblyLoaderEx
{
public:
	static Assembly^ AssemblyResolveEventHandler( Object^ sender, ResolveEventArgs^ args )
	{
		String^ dllName = args->Name->Substring(0, args->Name->IndexOf(','));
		if (dllName->Length > 0)
		{
			String ^strOwnPath = Assembly::GetAssembly(AssemblyLoaderEx::typeid)->Location;
			String ^strTargetPath = IO::Path::GetDirectoryName(strOwnPath) + "\\" + dllName + ".dll";

			if (IO::File::Exists(strTargetPath))
				return Assembly::LoadFile(strTargetPath);
		}
		
		return nullptr;
	}

	static void UnhandledExceptionHandler( Object^ sender, UnhandledExceptionEventArgs^ args )
	{
		String^ msgText = String::Format("ACHTUNG!!!! DANGER!!!! : {0}", args->ExceptionObject);
		
		msclr::interop::marshal_context ctx;
		MessageBox(0, ctx.marshal_as<const wchar_t*>(msgText), L"Global Error", MB_OK | MB_ICONERROR);
	}
};

struct VDisk
{
	gcroot<VirtualDisk^> pVdiskObj;
	gcroot< List<VDFileInfo^>^ > vItems;
	gcroot< List<String^>^ > vVolLabels; // Cache for volume labels
	bool filesEnumComplete;
};

static void EnumFilesInVolume(VDisk* vdObj, DiscDirectoryInfo^ dirInfo, LogicalVolumeInfo^ vol, int volIndex)
{
	try
	{
		array<DiscDirectoryInfo^> ^subDirList = dirInfo->GetDirectories();
		for (int i = 0; i < subDirList->Length; i++)
		{
			DiscDirectoryInfo^ subDir = subDirList[i];
			if (subDir->Name != "." && subDir->Name != "..")
			{
				vdObj->vItems->Add(gcnew VDFileInfo(subDir, volIndex));
				EnumFilesInVolume(vdObj, subDir, vol, volIndex);
			}
		}
	}
	catch (System::IO::IOException^)
	{
		//Usually this exception occurs on corrupted data
		//So we just skip all underlying directories
	}

	array<DiscFileInfo^> ^fileList = dirInfo->GetFiles();
	for (int i = 0; i < fileList->Length; i++)
	{
		VDFileInfo^ fileInfo = gcnew VDFileInfo(fileList[i], volIndex);
		vdObj->vItems->Add(fileInfo);
	}
}

static void PrepareFileList(VDisk* vdisk)
{
	VolumeManager^ volm = gcnew VolumeManager(vdisk->pVdiskObj);

	array<LogicalVolumeInfo^> ^logvol = volm->GetLogicalVolumes();
	for (int i = 0; i < logvol->Length; i++)
	{
		LogicalVolumeInfo^ volInfo = logvol[i];
		
		if (volInfo->Status != LogicalVolumeStatus::Healthy) continue;

		array<DiscUtils::FileSystemInfo^> ^fsinfo = FileSystemManager::DetectDefaultFileSystems(volInfo);
		if (fsinfo == nullptr || fsinfo->Length == 0) continue;

		FileSystemParameters^ fsParams = gcnew FileSystemParameters();
		fsParams->FileNameEncoding = System::Text::Encoding::GetEncoding(GetOEMCP());

		try
		{
			DiscFileSystem^ dfs = fsinfo[0]->Open(volInfo, fsParams);
			if (dfs->GetType() == Ntfs::NtfsFileSystem::typeid)
			{
				Ntfs::NtfsFileSystem^ ntfsFS = (Ntfs::NtfsFileSystem^)dfs;
				ntfsFS->NtfsOptions->HideHiddenFiles = false;
				ntfsFS->NtfsOptions->HideSystemFiles = false;
				//ntfsFS->NtfsOptions->HideMetafiles = false;
			}
			
			EnumFilesInVolume(vdisk, dfs->Root, volInfo, i);

			String^ volLabel = dfs->VolumeLabel->Trim();
			if (String::IsNullOrEmpty(volLabel)) volLabel = "Volume_#" + i;
			vdisk->vVolLabels->Add(volLabel);
		}
		catch (Exception^ ex)
		{
			String^ errText = String::Format("Volume listing error : {0}", ex);

			msclr::interop::marshal_context ctx;
			MessageBox(0, ctx.marshal_as<const wchar_t*>(errText), L"Error", MB_OK | MB_ICONERROR);
		}
	} // for
}

static ::FILETIME DateTimeToFileTime(DateTime dt)
{
	::FILETIME ft = {0};

	LARGE_INTEGER li;
	li.QuadPart = dt.ToFileTime();

	ft.dwLowDateTime = li.LowPart;
	ft.dwHighDateTime = li.HighPart;
	return ft;
}

const wchar_t* GetDiskType(VirtualDisk ^vdisk)
{
	if (vdisk->GetType() == Vmdk::Disk::typeid)
		return L"VMWare Virtual Hard Disk";
	else if (vdisk->GetType() == Vdi::Disk::typeid)
		return L"Virtual Box Hard Disk";
	else if (vdisk->GetType() == Vhd::Disk::typeid)
		return L"Microsoft Virtual Hard Disk";
	else if (vdisk->GetType() == Xva::Disk::typeid)
		return L"Xen Virtual Appliance Disk";

	return L"Virtual Disk";
}

int MODULE_EXPORT OpenStorage(StorageOpenParams params, HANDLE *storage, StorageGeneralInfo* info)
{
	String ^strPath = gcnew String(params.FilePath);
	VirtualDisk ^vdisk;
	try
	{
		vdisk = VirtualDisk::OpenDisk(strPath, IO::FileAccess::Read);
	}
	catch (Exception^)
	{
		vdisk = nullptr;
	}

	if (vdisk != nullptr)
	{
		bool fHaveValidVolumes = false;

		// Verify that we have any recognizable volumes
		VolumeManager^ volm = gcnew VolumeManager(vdisk);
		array<LogicalVolumeInfo^> ^logvol = volm->GetLogicalVolumes();
		for (int i = 0; i < logvol->Length; i++)
		{
			// Skip failed volumes
			if (logvol[i]->Status != LogicalVolumeStatus::Healthy) continue;
			
			array<DiscUtils::FileSystemInfo^> ^fsinfo = FileSystemManager::DetectDefaultFileSystems(logvol[i]);
			if (fsinfo != nullptr && fsinfo->Length > 0)
			{
				fHaveValidVolumes = true;
				break;
			}
		}

		if (fHaveValidVolumes)
		{
			VDisk* vdObj = new VDisk();
			vdObj->pVdiskObj = vdisk;
			vdObj->vItems = gcnew List<VDFileInfo^>();
			vdObj->vVolLabels = gcnew List<String^>();
			vdObj->filesEnumComplete = false;

			*storage = vdObj;

			memset(info, 0, sizeof(StorageGeneralInfo));
			wcscpy_s(info->Format, STORAGE_FORMAT_NAME_MAX_LEN, GetDiskType(vdisk));
			wcscpy_s(info->Comment, STORAGE_PARAM_MAX_LEN, L"-");
			wcscpy_s(info->Compression, STORAGE_PARAM_MAX_LEN, L"None");

			return SOR_SUCCESS;
		}
	}

	if (vdisk != nullptr)
	{
		delete vdisk;
		vdisk = nullptr;
	}

	return SOR_INVALID_FILE;
}

void MODULE_EXPORT CloseStorage(HANDLE storage)
{
	VDisk* vdObj = (VDisk*) storage;
	if (vdObj != NULL)
	{
		vdObj->vVolLabels->Clear();
		vdObj->vVolLabels = nullptr;
		
		vdObj->vItems->Clear();
		vdObj->vItems = nullptr;

		delete vdObj->pVdiskObj;
		vdObj->pVdiskObj = nullptr;
		
		delete vdObj;
	}
}

int MODULE_EXPORT GetStorageItem(HANDLE storage, int item_index, StorageItemInfo* item_info)
{
	VDisk* vdObj = (VDisk*) storage;
	if (vdObj == NULL || item_index < 0) return GET_ITEM_ERROR;

	if (!vdObj->filesEnumComplete)
	{
		PrepareFileList(vdObj);
		vdObj->filesEnumComplete = true;
	}

	if (item_index >= vdObj->vItems->Count)
		return GET_ITEM_NOMOREITEMS;

	List<VDFileInfo^> ^fileList = vdObj->vItems;
	VDFileInfo^ fileInfo = fileList[item_index];
	
	List<String^> ^volLabels = vdObj->vVolLabels;
	String^ filePath = String::Format("[{0}]\\{1}", volLabels[fileInfo->VolumeIndex], fileInfo->Ref->FullName);
	
	// Remove trailing backslash if present
	if (filePath->EndsWith("\\"))
		filePath = filePath->Remove(filePath->Length - 1);

	// Helper class for String^ to wchar_t* conversion
	msclr::interop::marshal_context ctx;

	memset(item_info, 0, sizeof(StorageItemInfo));
	wcscpy_s(item_info->Path, STRBUF_SIZE(item_info->Path), ctx.marshal_as<const wchar_t*>(filePath));
	item_info->Attributes = (DWORD) fileInfo->Ref->Attributes;
	item_info->Size = (fileInfo->Ref->GetType() == DiscFileInfo::typeid) ? ((DiscFileInfo^)fileInfo->Ref)->Length : 0;;
	item_info->CreationTime = DateTimeToFileTime(fileInfo->Ref->CreationTimeUtc);
	item_info->ModificationTime = DateTimeToFileTime(fileInfo->Ref->LastWriteTimeUtc);
	
	return GET_ITEM_OK;
}

int MODULE_EXPORT ExtractItem(HANDLE storage, ExtractOperationParams params)
{
	VDisk* vdObj = (VDisk*) storage;
	if (vdObj == NULL || params.item < 0 || params.item >= vdObj->vItems->Count || !params.destFilePath)
		return SER_ERROR_SYSTEM;

	List<VDFileInfo^> ^fileList = vdObj->vItems;
	VDFileInfo^ fileInfo = fileList[params.item];
	
	// We do not extract directories
	if (fileInfo->Ref->GetType() != DiscFileInfo::typeid)
		return SER_ERROR_SYSTEM;
	
	DiscFileInfo^ dfi = (DiscFileInfo^) fileInfo->Ref;
	if (!dfi->Exists) return SER_ERROR_READ;

	String^ strDestFile = gcnew String(params.destFilePath);
	int result = SER_SUCCESS;
	
	try
	{
		IO::Stream ^inStr = dfi->OpenRead();
		IO::Stream ^outStr = gcnew IO::FileStream(strDestFile, IO::FileMode::Create, IO::FileAccess::Write, IO::FileShare::Read);

		int copyBufSize = 32 * 1024;
		array<Byte> ^copyBuf = gcnew array<Byte>(copyBufSize);
		
		Int64 bytesLeft = dfi->Length;
		while (bytesLeft > 0)
		{
			int copySize = (int) Math::Min(bytesLeft, (Int64) copyBufSize);
			int readNum = inStr->Read(copyBuf, 0, copySize);

			if (readNum != copySize)
			{
				result = SER_ERROR_READ;
				break;
			}
			
			outStr->Write(copyBuf, 0, readNum);
			bytesLeft -= readNum;
			
			if (params.callbacks.FileProgress)
				if (!params.callbacks.FileProgress(params.callbacks.signalContext, readNum))
				{
					result = SER_USERABORT;
					break;
				}
		}
		
		outStr->Close();
		inStr->Close();
	}
	catch (...)
	{
		result = SER_ERROR_WRITE;
	}

	if (result != SER_SUCCESS && IO::File::Exists(strDestFile))
	{
		IO::File::Delete(strDestFile);
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Exported Functions
//////////////////////////////////////////////////////////////////////////

int MODULE_EXPORT LoadSubModule(ModuleLoadParameters* LoadParams)
{
	LoadParams->ModuleVersion = MAKEMODULEVERSION(1, 1);
	LoadParams->ApiVersion = ACTUAL_API_VERSION;
	LoadParams->OpenStorage = OpenStorage;
	LoadParams->CloseStorage = CloseStorage;
	LoadParams->GetItem = GetStorageItem;
	LoadParams->ExtractItem = ExtractItem;

	try
	{
		AppDomain::CurrentDomain->AssemblyResolve += gcnew ResolveEventHandler(AssemblyLoaderEx::AssemblyResolveEventHandler);
		AppDomain::CurrentDomain->UnhandledException += gcnew UnhandledExceptionEventHandler(AssemblyLoaderEx::UnhandledExceptionHandler);

		return TRUE;
	}
	catch (System::Security::SecurityException^ sEx)
	{
		msclr::interop::marshal_context ctx;
		MessageBox(0, ctx.marshal_as<const wchar_t*>(sEx->Message), L"Module Loading Error", MB_OK | MB_ICONERROR);

		return FALSE;
	}
}

void MODULE_EXPORT UnloadSubModule()
{
}
