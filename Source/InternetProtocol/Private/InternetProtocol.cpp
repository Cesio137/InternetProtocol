// Copyright Epic Games, Inc. All Rights Reserved.

#include "InternetProtocol.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FInternetProtocolModule"

void FInternetProtocolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	/*
	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("InternetProtocol")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString LegacyLibraryPath;
	FString CryptoLibraryPath;
	FString SslLibraryPath;
#if PLATFORM_WINDOWS
	LegacyLibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/openssl/Win64/legacy.dll"));
	CryptoLibraryPath= FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/openssl/Win64/libcrypto-3-x64.dll"));
	SslLibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/openssl/Win64/libssl-3-x64.dll"));
#elif PLATFORM_MAC
	LibraryPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/SocketIO_MMOLibrary/Mac/Release/libExampleLibrary.dylib"));
#elif PLATFORM_LINUX
	LibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/SocketIO_MMOLibrary/Linux/x86_64-unknown-linux-gnu/libExampleLibrary.so"));
#endif // PLATFORM_WINDOWS

	LegacyLibraryHandle = !LegacyLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LegacyLibraryPath) : nullptr;
	CryptoLibraryHandle = !CryptoLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*CryptoLibraryPath) : nullptr;
	SslLibraryHandle = !SslLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*SslLibraryPath) : nullptr;

	if (!LegacyLibraryHandle)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Openssl Legacy Error.", "Failed to load Openssl Legacy library."));
	}
	if (!CryptoLibraryHandle)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Openssl Crypto Error.", "Failed to load Openssl Crypto library."));
	}
	if (!SslLibraryHandle)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Openssl SSL Error.", "Failed to load Openssl SSL library."));
	}
	*/
}

void FInternetProtocolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	/*
	// Free the dll handle
	FPlatformProcess::FreeDllHandle(LegacyLibraryHandle);
	LegacyLibraryHandle = nullptr;

	FPlatformProcess::FreeDllHandle(CryptoLibraryHandle);
	CryptoLibraryHandle = nullptr;
	
	FPlatformProcess::FreeDllHandle(SslLibraryHandle);
	SslLibraryHandle = nullptr;
	*/
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInternetProtocolModule, InternetProtocol)