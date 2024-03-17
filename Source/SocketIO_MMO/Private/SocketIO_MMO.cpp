// Copyright Epic Games, Inc. All Rights Reserved.

#include "SocketIO_MMO.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "sio_client.h"

#define LOCTEXT_NAMESPACE "FSocketIO_MMOModule"

void FSocketIO_MMOModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("SocketIO_MMO")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString SioclientLibraryPath;
#if PLATFORM_WINDOWS
	SioclientLibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/SocketIO_Library/Win64/sioclient.dll"));
#elif PLATFORM_MAC
    //LibraryPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/SocketIO_MMOLibrary/Mac/Release/libExampleLibrary.dylib"));
#elif PLATFORM_LINUX
	//LibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/SocketIO_MMOLibrary/Linux/x86_64-unknown-linux-gnu/libExampleLibrary.so"));
#endif // PLATFORM_WINDOWS

	SioclientLibraryHandle = !SioclientLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*SioclientLibraryPath) : nullptr;

	
	if (!SioclientLibraryHandle)
	{
		// Call the test function in the third party library that opens a message box
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Sioclient error!", "Failed to load sioclient library!"));
	}
	
}

void FSocketIO_MMOModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Free the dll handle
	FPlatformProcess::FreeDllHandle(SioclientLibraryHandle);
	SioclientLibraryHandle = nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSocketIO_MMOModule, SocketIO_MMO)
