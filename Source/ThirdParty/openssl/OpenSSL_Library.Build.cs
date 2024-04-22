// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class OpenSSL_Library : ModuleRules
{
	public OpenSSL_Library(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add the import library
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win_x64", "libcrypto.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "Win_x64", "libssl.lib"));

			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add("legacy.dll");
			PublicDelayLoadDLLs.Add("libcrypto-3-x64.dll");
			PublicDelayLoadDLLs.Add("libssl-3-x64.dll");

			// Ensure that the DLL is staged along with the executable
			RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/openssl/Win64/legacy.dll");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/openssl/Win64/libcrypto-3-x64.dll");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/ThirdParty/openssl/Win64/libssl-3-x64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "Mac", "Release", "libExampleLibrary.dylib"));
			RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/SocketIO_Library/Mac/Release/libExampleLibrary.dylib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string ExampleSoPath = Path.Combine("$(PluginDir)", "Binaries", "ThirdParty", "SocketIO_MMOLibrary", "Linux", "x86_64-unknown-linux-gnu", "libExampleLibrary.so");
			PublicAdditionalLibraries.Add(ExampleSoPath);
			PublicDelayLoadDLLs.Add(ExampleSoPath);
			RuntimeDependencies.Add(ExampleSoPath);
		}
	}
}