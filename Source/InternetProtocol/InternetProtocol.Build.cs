/** 
 * Copyright © 2025 Nathan Miguel
 */

using UnrealBuildTool;

public class InternetProtocol : ModuleRules
{
	public InternetProtocol(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 6) {
			CppStandard = CppStandardVersion.Default;
		}
		else {
			CppStandard = CppStandardVersion.Cpp17;
		}
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add("ASIO_NO_EXCEPTIONS");
		
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