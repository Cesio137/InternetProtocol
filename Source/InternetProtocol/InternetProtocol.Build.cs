/*
 * Copyright (c) 2023-2024 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

using UnrealBuildTool;

public class InternetProtocol : ModuleRules
{
	public InternetProtocol(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp17;
		PublicDefinitions.AddRange(new string[]
		{
			"ASIO_STANDALONE",
			"ASIO_NO_DEPRECATED",
			"ASIO_NO_EXCEPTIONS",
			"ASIO_DISABLE_CXX11_MACROS",
			"NOMINMAX",
		});
		
		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicDefinitions.AddRange(new string[]
			{
				"WIN32_LEAN_AND_MEAN",
			});
			if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicDefinitions.AddRange(new string[]
				{
					"_WIN32_WINNT 0x0A00",
				});
			}
		}
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				"$(PluginDir)/Source/ThirdParty/asio/Public",
            }
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"OpenSSL",
                "Json",
                "JsonUtilities",
                "Projects",
                "CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
