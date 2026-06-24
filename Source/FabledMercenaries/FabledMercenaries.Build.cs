// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FabledMercenaries : ModuleRules
{
	public FabledMercenaries(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"FabledMercenaries",
			"FabledMercenaries/Variant_Strategy",
			"FabledMercenaries/Variant_Strategy/UI",
			"FabledMercenaries/Variant_TwinStick",
			"FabledMercenaries/Variant_TwinStick/AI",
			"FabledMercenaries/Variant_TwinStick/Gameplay",
			"FabledMercenaries/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
