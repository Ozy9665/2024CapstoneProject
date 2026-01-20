// Fill out your copyright notice in the Description page of Project Settings.


#include "NavMeshExportBPLibrary.h"

#if WITH_EDITOR

#include "Editor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/NavDataGenerator.h"

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