// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class HearthvaleEditorTarget : TargetRules
{
	public HearthvaleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Hearthvale");

		// The Fab plugin updates itself apart from the engine, and its 2026-10-28 update added
		// a module the installed engine's precompiled build rules don't know, which stops every
		// build. It's prebuilt and still loads in the editor; we just don't build against it.
		DisablePlugins.Add("Fab");
	}
}
