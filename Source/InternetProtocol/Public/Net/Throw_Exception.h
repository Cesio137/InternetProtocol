#pragma once

#include "CoreMinimal.h"
#include "Net/Commons.h"
#include "Logging/LogMacros.h"
#define HANDLE_ERROR(Error_Code) handle_error(Error_Code, __FILE__, __LINE__);
DEFINE_LOG_CATEGORY_STATIC(LogAsio, Log, All);


namespace asio::detail
{
	template <typename Exception>
	inline void throw_exception(const Exception& e) noexcept
	{
		UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR>"));
		UE_LOG(LogAsio, Warning, TEXT("%s"), *FString(e.what()));
		UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR/>"));
	}
}
