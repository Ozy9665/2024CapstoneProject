// Fill out your copyright notice in the Description page of Project Settings.


#include "NavMeshExportBPLibrary.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Detour/DetourNavMesh.h"
#include "AI/NavDataGenerator.h"


static const int32 NAVMESHSET_MAGIC = ('M' << 24) | ('S' << 16) | ('E' << 8) | ('T');
static const int32 NAVMESHSET_VERSION = 1;

struct FDetourNavMeshSetHeader
{
	int32 Magic = NAVMESHSET_MAGIC;
	int32 Version = NAVMESHSET_VERSION;
	int32 NumTiles = 0;
	dtNavMeshParams Params{};
};

struct FDetourNavMeshTileHeader
{
	dtTileRef TileRef = 0;
	int32 DataSize = 0;
};

static bool WriteBytes(FArchive& Ar, const void* Data, int64 Num)
{
	Ar.Serialize(const_cast<void*>(Data), Num);
	return !Ar.IsError();
}

bool UNavMeshExportBPLibrary::ExportDetourNavMeshBIN(const FString& OutputBinPath, FString& OutError)
{
	OutError.Reset();

	if (!GEditor)
	{
		OutError = TEXT("GEditor is null. (Editor-only)");
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = TEXT("Editor World is null.");
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		OutError = TEXT("NavigationSystem is null.");
		return false;
	}

	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	ARecastNavMesh* RecastNav = Cast<ARecastNavMesh>(NavData);
	if (!RecastNav)
	{
		OutError = TEXT("Default NavData is not ARecastNavMesh. (Is Recast NavMesh used / built?)");
		return false;
	}

	const FPImplRecastNavMesh* PImpl = RecastNav->GetRecastNavMeshImpl();
	if (!PImpl || !PImpl->DetourNavMesh)
	{
		OutError = TEXT("DetourNavMesh is null. Did you build navmesh? (Build > Build Paths)");
		return false;
	}

	const dtNavMesh* DNav = PImpl->DetourNavMesh;

	// 출력 폴더 보장
	const FString AbsPath = FPaths::ConvertRelativePathToFull(OutputBinPath);
	const FString Dir = FPaths::GetPath(AbsPath);

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.DirectoryExists(*Dir))
	{
		PF.CreateDirectoryTree(*Dir);
	}

	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*AbsPath));
	if (!Ar)
	{
		OutError = FString::Printf(TEXT("Failed to open for write: %s"), *AbsPath);
		return false;
	}

	// 헤더 작성
	FDetourNavMeshSetHeader Header;
	Header.Params = *DNav->getParams();

	// 타일 개수 계산
	const int32 MaxTiles = DNav->getMaxTiles();
	for (int32 i = 0; i < MaxTiles; ++i)
	{
		const dtMeshTile* Tile = DNav->getTile(i);
		if (!Tile || !Tile->header || !Tile->dataSize) continue;
		Header.NumTiles++;
	}

	if (!WriteBytes(*Ar, &Header, sizeof(Header)))
	{
		OutError = TEXT("Failed to write navmesh header.");
		return false;
	}

	// 타일들 저장 (tileRef + dataSize + raw data)
	for (int32 i = 0; i < MaxTiles; ++i)
	{
		const dtMeshTile* Tile = DNav->getTile(i);
		if (!Tile || !Tile->header || !Tile->dataSize) continue;

		FDetourNavMeshTileHeader TH;
		TH.TileRef = DNav->getTileRef(Tile);
		TH.DataSize = (int32)Tile->dataSize;

		if (!WriteBytes(*Ar, &TH, sizeof(TH)))
		{
			OutError = TEXT("Failed to write tile header.");
			return false;
		}

		if (!WriteBytes(*Ar, Tile->data, TH.DataSize))
		{
			OutError = TEXT("Failed to write tile data.");
			return false;
		}
	}

	Ar->Close();

	// 파일 생성 확인
	if (!PF.FileExists(*AbsPath))
	{
		OutError = FString::Printf(TEXT("Wrote file but it does not exist: %s"), *AbsPath);
		return false;
	}

	return true;
}

bool UNavMeshExportBPLibrary::ExportNavMeshOBJ(const FString& OutputObjPath, FString& OutError)
{
	OutError.Reset();

	if (!GEditor)
	{
		OutError = TEXT("GEditor is null. (Editor-only)");
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = TEXT("Editor World is null.");
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		OutError = TEXT("NavigationSystem is null.");
		return false;
	}

	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	ARecastNavMesh* RecastNav = Cast<ARecastNavMesh>(NavData);
	if (!RecastNav)
	{
		OutError = TEXT("Default NavData is not ARecastNavMesh. (Is Recast NavMesh used / built?)");
		return false;
	}

	FNavDataGenerator* Gen = RecastNav->GetGenerator();
	if (!Gen)
	{
		OutError =
			TEXT("RecastNavMesh Generator is null.\n")
			TEXT("Fix:\n")
			TEXT("1) Project Settings > Navigation Mesh > Runtime Generation = Dynamic\n")
			TEXT("2) Build > Build Paths\n")
			TEXT("3) Save the level\n");
		return false;
	}

	// 폴더가 없으면 만들어주기
	const FString AbsPath = FPaths::ConvertRelativePathToFull(OutputObjPath);
	const FString Dir = FPaths::GetPath(AbsPath);

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.DirectoryExists(*Dir))
	{
		PF.CreateDirectoryTree(*Dir);
	}

	Gen->ExportNavigationData(AbsPath);

	// Export 후 파일이 생성됐는지
	IPlatformFile& PF2 = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF2.FileExists(*AbsPath))
	{
		OutError = FString::Printf(TEXT("ExportNavigationData executed, but file not found. Path: %s"), *AbsPath);
		return false;
	}

	return true;
}

#endif 