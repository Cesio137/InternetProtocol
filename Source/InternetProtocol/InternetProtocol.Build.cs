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
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Version.MajorVersion == 4)
		{
			PublicDefinitions.AddRange(
				new string[]
				{
					"ASIO_STANDALONE",
					"ASIO_NO_DEPRECATED",
					"ASIO_NO_EXCEPTIONS"
				}
			);
		}
		
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
				"$(PluginDir)/Source/ThirdParty/asio/Public"
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
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
				"SlateCore"
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