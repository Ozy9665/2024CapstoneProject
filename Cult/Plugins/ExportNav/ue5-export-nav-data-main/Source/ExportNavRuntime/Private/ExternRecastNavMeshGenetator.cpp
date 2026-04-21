
#include "ExternRecastNavMeshGenetator.h"

#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/NavDataGenerator.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include "Detour/DetourNavMesh.h"

#include "Resources/Version.h"

static FORCEINLINE FVector RecastToUnrealXYZ(double X, double Y, double Z)
{
	// Recast(x,y,z) -> Unreal = (-x, -z, y)
	return FVector((float)-X, (float)-Z, (float)Y);
}

static FORCEINLINE FVector UnrealToRecast(const FVector& V)
{
	return FVector(-V.X, V.Z, -V.Y);
}


static void ExportDetourNavMeshPolysToOBJ(
	const FString& InFileName,
	const ARecastNavMesh* NavData,
	const bool bExportMeters,
	const FString& AdditionalData)
{
#if ALLOW_DEBUG_FILES
	if (!NavData)
	{
		UE_LOG(LogNavigation, Error, TEXT("[ExportNav] NavData is null."));
		return;
	}

	const dtNavMesh* Mesh = NavData->GetRecastMesh();
	if (!Mesh)
	{
		UE_LOG(LogNavigation, Error, TEXT("[ExportNav] NavData->GetRecastMesh() returned null. (NavMesh not built?)"));
		return;
	}

	// Ensure directory exists (optional but helpful)
	{
		const FString AbsPath = FPaths::ConvertRelativePathToFull(InFileName);
		const FString Dir = FPaths::GetPath(AbsPath);
		IFileManager::Get().MakeDirectory(*Dir, true);
	}

	FArchive* FileAr = IFileManager::Get().CreateDebugFileWriter(*InFileName);
	if (!FileAr)
	{
		UE_LOG(LogNavigation, Error, TEXT("[ExportNav] Failed to open file for writing: %s"), *InFileName);
		return;
	}

	int32 GlobalVertBase = 1; // OBJ index starts at 1

	const int32 MaxTiles = Mesh->getMaxTiles();
	int32 ExportedTiles = 0;
	int32 ExportedPolys = 0;
	int32 ExportedVerts = 0;

	for (int32 TileIdx = 0; TileIdx < MaxTiles; ++TileIdx)
	{
		const dtMeshTile* Tile = Mesh->getTile(TileIdx);
		if (!Tile || !Tile->header) continue;

		ExportedTiles++;

		// 1) Write tile vertices
		for (int32 Vi = 0; Vi < Tile->header->vertCount; ++Vi)
		{
			const int32 Base = Vi * 3;
			FVector Ue = RecastToUnrealXYZ(
				(double)Tile->verts[Base + 0],
				(double)Tile->verts[Base + 1],
				(double)Tile->verts[Base + 2]
			);

			if (bExportMeters)
			{
				Ue /= 100.f; 
			}

			const FString Line = FString::Printf(TEXT("v %f %f %f\n"), Ue.X, Ue.Y, Ue.Z);
			auto Ansi = StringCast<ANSICHAR>(*Line);
			FileAr->Serialize((ANSICHAR*)Ansi.Get(), Ansi.Length());
		}
		ExportedVerts += Tile->header->vertCount;

		// 2) Write faces from polys (fan triangulation)
		for (int32 Pi = 0; Pi < Tile->header->polyCount; ++Pi)
		{
			const dtPoly& Poly = Tile->polys[Pi];

			const int32 N = (int32)Poly.vertCount;
			if (N < 3) continue;

			// 폴리의 vertex index들이 타일 vertex 범위를 벗어나면 스킵 (안전장치)
			bool bValid = true;
			for (int32 k = 0; k < N; ++k)
			{
				if (Poly.verts[k] >= Tile->header->vertCount)
				{
					bValid = false;
					break;
				}
			}
			if (!bValid) continue;

			const int32 V0 = GlobalVertBase + (int32)Poly.verts[0];
			for (int32 k = 1; k < N - 1; ++k)
			{
				const int32 V1 = GlobalVertBase + (int32)Poly.verts[k];
				const int32 V2 = GlobalVertBase + (int32)Poly.verts[k + 1];

				const FString Line = FString::Printf(TEXT("f %d %d %d\n"), V0, V1, V2);
				auto Ansi = StringCast<ANSICHAR>(*Line);
				FileAr->Serialize((ANSICHAR*)Ansi.Get(), Ansi.Length());
			}

			ExportedPolys++;
		}

		GlobalVertBase += Tile->header->vertCount;
	}

	if (!AdditionalData.IsEmpty())
	{
		auto AnsiAdditionalData = StringCast<ANSICHAR>(*AdditionalData);
		FileAr->Serialize((ANSICHAR*)AnsiAdditionalData.Get(), AnsiAdditionalData.Length());
	}

	FileAr->Close();

	UE_LOG(LogNavigation, Log, TEXT("[ExportNav] Exported NavMesh OBJ: %s | Tiles=%d Polys=%d Verts=%d"),
		*InFileName, ExportedTiles, ExportedPolys, ExportedVerts);

#endif // ALLOW_DEBUG_FILES
}


FExternRecastGeometryCache::FExternRecastGeometryCache(const uint8* Memory)
{
	Header = *((FHeader*)Memory);
	Verts = (dtReal*)(Memory + sizeof(FExternRecastGeometryCache));
	Indices = (int32*)(Memory + sizeof(FExternRecastGeometryCache) + (sizeof(dtReal) * Header.NumVerts * 3));
}


void FExternExportNavMeshGenerator::ExternExportNavigationData(const FString& FileName, EExportMode InExportMode)
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogNavigation, Error, TEXT("[ExportNav] NavigationSystem is null."));
		return;
	}

	const double StartExportTime = FPlatformTime::Seconds();
	const bool bMeters = (InExportMode == EExportMode::Metre);

	// NavDataSet 안의 RecastNavMesh들을 돌면서 각각 export
	for (int32 Index = 0; Index < NavSys->NavDataSet.Num(); ++Index)
	{
		const ARecastNavMesh* NavData = Cast<const ARecastNavMesh>(NavSys->NavDataSet[Index]);
		if (!NavData) continue;

		// AdditionalData 만들기 (너가 원하면 더 줄이거나 늘릴 수 있음)
		FString AdditionalData;
		AdditionalData += "# RecastDemo specific data\n";

		{
			const FVector UCenter = NavData->GetBounds().GetCenter();
			const FVector UExtent = NavData->GetBounds().GetExtent();

			const FVector RCCenter = UnrealToRecast(UCenter);
			const FVector RCExtent = FVector(FMath::Abs(UnrealToRecast(UExtent).X), FMath::Abs(UnrealToRecast(UExtent).Y), FMath::Abs(UnrealToRecast(UExtent).Z));

			const FBox Box = FBox::BuildAABB(RCCenter, RCExtent);
			AdditionalData += FString::Printf(
				TEXT("rd_bbox %7.7f %7.7f %7.7f %7.7f %7.7f %7.7f\n"),
				Box.Min.X, Box.Min.Y, Box.Min.Z,
				Box.Max.X, Box.Max.Y, Box.Max.Z
			);
		}

		if (const FRecastNavMeshGenerator* CurrentGen = static_cast<const FRecastNavMeshGenerator*>(NavData->GetGenerator()))
		{
			AdditionalData += FString::Printf(TEXT("# Cell Size\n"));
			AdditionalData += FString::Printf(TEXT("rd_cs %5.5f\n"), CurrentGen->GetConfig().cs);
			AdditionalData += FString::Printf(TEXT("# Cell Height\n"));
			AdditionalData += FString::Printf(TEXT("rd_ch %5.5f\n"), CurrentGen->GetConfig().ch);
			AdditionalData += FString::Printf(TEXT("# Agent max slope\n"));
			AdditionalData += FString::Printf(TEXT("rd_ams %5.5f\n"), CurrentGen->GetConfig().walkableSlopeAngle);
			AdditionalData += FString::Printf(TEXT("# Tile size\n"));
			AdditionalData += FString::Printf(TEXT("rd_ts %d\n"), CurrentGen->GetConfig().tileSize);
			AdditionalData += FString::Printf(TEXT("\n"));
		}

		ExportDetourNavMeshPolysToOBJ(FileName, NavData, bMeters, AdditionalData);
	}

	UE_LOG(LogNavigation, Log, TEXT("[ExportNav] ExportNavigation finished in %.3f sec."), FPlatformTime::Seconds() - StartExportTime);
}


void FExternExportNavMeshGenerator::GrowConvexHull(const dtReal ExpandBy, const TArray<FVector>& Verts, TArray<FVector>& OutResult)
{
	
}

void FExternExportNavMeshGenerator::TransformVertexSoupToRecast(const TArray<FVector>& VertexSoup, TNavStatArray<FVector>& Verts, TNavStatArray<int32>& Faces)
{
	
}

void FExternExportNavMeshGenerator::ExportGeomToOBJFile(const FString& InFileName, const TNavStatArray<dtReal>& GeomCoords, const TNavStatArray<int32>& GeomFaces, const FString& AdditionalData)
{
	
}