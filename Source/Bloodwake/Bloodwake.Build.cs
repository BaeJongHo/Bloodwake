// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Bloodwake : ModuleRules
{
	public Bloodwake(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// flat 모듈(Public/Private 폴더 없음)이라 모듈 루트를 명시적으로 include 경로에 등록한다.
		// 이로써 서브폴더 헤더를 "GameMode/BWGameMode.h" 같은 모듈 루트 기준 경로로 include 할 수 있다.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "GameplayTags" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
