// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Hearthvale : ModuleRules
{
	public Hearthvale(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Hearthvale",
			"Hearthvale/Variant_Platforming",
			"Hearthvale/Variant_Platforming/Animation",
			"Hearthvale/Variant_Combat",
			"Hearthvale/Variant_Combat/AI",
			"Hearthvale/Variant_Combat/Animation",
			"Hearthvale/Variant_Combat/Gameplay",
			"Hearthvale/Variant_Combat/Interfaces",
			"Hearthvale/Variant_Combat/UI",
			"Hearthvale/Variant_SideScrolling",
			"Hearthvale/Variant_SideScrolling/AI",
			"Hearthvale/Variant_SideScrolling/Gameplay",
			"Hearthvale/Variant_SideScrolling/Interfaces",
			"Hearthvale/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
