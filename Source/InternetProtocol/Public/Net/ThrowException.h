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

#pragma once

#include "Net/Commons.h"
#ifdef ASIO_NO_EXCEPTIONS
#include "Logging/LogMacros.h"
#define HANDLE_ERROR(Error_Code) handle_error(Error_Code, __FILE__, __LINE__);
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
#endif