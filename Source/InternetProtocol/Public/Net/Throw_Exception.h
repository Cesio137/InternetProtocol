#pragma once

#include "Net/Commons.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAsio, Log, All);

namespace asio::detail
{
	template <typename Exception>
	inline void throw_exception(const Exception& e)
	{
		UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR>"));
		UE_LOG(LogAsio, Warning, TEXT("%s"), *FString(e.what()));
		UE_LOG(LogAsio, Error, TEXT("<ASIO ERROR/>"));
	}
}
