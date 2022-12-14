#pragma once
#include <vector>

namespace Carol
{
	class Bitset
	{
	public:
		Bitset(uint32_t numPages);

		bool Set(uint32_t idx);
		bool Reset(uint32_t idx);
		bool IsPageIdle(uint32_t idx);
	private:
		std::vector<uint64_t> mBitset;
		uint32_t mNumPages;
	};
}
