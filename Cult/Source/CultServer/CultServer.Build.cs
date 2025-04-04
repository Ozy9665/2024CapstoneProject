// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class CultServer : ModuleRules
{
    public CultServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Sockets",
                "Networking"
            }
        );
    }
}
