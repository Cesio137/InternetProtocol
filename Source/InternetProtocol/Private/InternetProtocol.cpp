//© Nathan Miguel, 2024. All Rights Reserved.

#include "InternetProtocol.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FInternetProtocolModule"

void FInternetProtocolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("InternetProtocol")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString LegacyLibraryPath;
	FString LibcryptoLibraryPath;
	FString Libssl3LibraryPath;

#if PLATFORM_WINDOWS
	LegacyLibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/Win64/legacy.dll"));
	LibcryptoLibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/Win64/libcrypto-3-x64.dll"));
	Libssl3LibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/Win64/libssl-3-x64.dll"));
#endif
	LegacyLibraryHandle = !LegacyLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LegacyLibraryPath) : nullptr;
	LibcryptoLibraryHandle = !LibcryptoLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibcryptoLibraryPath) : nullptr;
	Libssl3LibraryHandle = !Libssl3LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*Libssl3LibraryPath) : nullptr;
	
	if (!LegacyLibraryHandle)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("openssl library error!", "Failed to load legacy."));
	}
	if (!LibcryptoLibraryHandle)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("openssl library error!", "Failed to load libcrypto-3-x64."));
	}
	if (!Libssl3LibraryHandle)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("openssl library error!", "Failed to load libssl-3-x64."));
	}
}

void FInternetProtocolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FPlatformProcess::FreeDllHandle(LegacyLibraryHandle);
	LegacyLibraryHandle = nullptr;

	FPlatformProcess::FreeDllHandle(LibcryptoLibraryHandle);
	LibcryptoLibraryHandle = nullptr;

	FPlatformProcess::FreeDllHandle(Libssl3LibraryHandle);
	Libssl3LibraryHandle = nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInternetProtocolModule, InternetProtocol)