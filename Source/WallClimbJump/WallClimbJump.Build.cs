// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WallClimbJump : ModuleRules
{
	public WallClimbJump(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "CableComponent" });
	}
}
