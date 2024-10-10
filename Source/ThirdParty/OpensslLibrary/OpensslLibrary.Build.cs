// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class OpensslLibrary : ModuleRules
{
	public OpensslLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add the import library
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win64", "libcrypto.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win64", "libssl.lib"));

			// Delay-load the DLL, so we can load it from the right place first
			//PublicDelayLoadDLLs.Add("steam_api64.dll");

			// Ensure that the DLL is staged along with the executable
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/legacy.dll");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/libcrypto-3-x64.dll");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/libssl-3-x64.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// Add the import library
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Linux", "libcrypto.a"));
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Linux", "libssl.a"));
		}
	}
}
