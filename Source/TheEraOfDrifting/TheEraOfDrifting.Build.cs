// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TheEraOfDrifting : ModuleRules
{
	public TheEraOfDrifting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "LevelSequence",
            "SlateCore",
            "MovieScene",
            "TakeMovieScene",
            "Sequencer",
            "TakeRecorderSources",
            "TakesCore",
            "TakeRecorder",
            "TakeTrackRecorders",
            "AssetRegistry",
            "SerializedRecorderInterface"
        });
    }
}
