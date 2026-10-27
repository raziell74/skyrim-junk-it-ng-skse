#pragma once

#include <cstdint>

namespace InventoryInjectorImproved::API
{
	class IAPI
	{
	public:
		IAPI() = default;
		virtual ~IAPI() = default;
		IAPI(const IAPI&) = delete;
		IAPI(IAPI&&) = delete;
		IAPI& operator=(const IAPI&) = delete;
		IAPI& operator=(IAPI&&) = delete;

		[[nodiscard]] virtual std::uint32_t GetVersion() const noexcept = 0;

		virtual void InvalidateItem(std::uint32_t a_formID) noexcept = 0;
	};

	constexpr std::uint32_t kVersion = 1;
	constexpr std::uint32_t kMessage_GetAPI = 'I5AP';
}
