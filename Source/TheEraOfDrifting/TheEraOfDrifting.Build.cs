// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TheEraOfDrifting : ModuleRules
{
	public TheEraOfDrifting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "LevelSequence", "MovieScene" });
	}
}
