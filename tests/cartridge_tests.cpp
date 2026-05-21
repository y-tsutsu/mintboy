#include "test.hpp"

#include "mintboy/cartridge.hpp"

#include <algorithm>
#include <vector>

namespace
{
    std::vector<mintboy::Byte> MakeRomWithHeader()
    {
        std::vector<mintboy::Byte> rom(0x8000, 0);
        const std::string title = "MINTBOY";
        std::copy(title.begin(), title.end(), rom.begin() + 0x0134);

        rom[0x0147] = 0x00;
        rom[0x0148] = 0x00;
        rom[0x0149] = 0x00;

        mintboy::Byte checksum = 0;
        for (std::size_t address = 0x0134; address <= 0x014C; ++address)
        {
            checksum = static_cast<mintboy::Byte>(checksum - rom[address] - 1);
        }
        rom[0x014D] = checksum;

        return rom;
    }
}

MINTBOY_TEST(cartridge_reads_header_fields)
{
    const mintboy::Cartridge cartridge(MakeRomWithHeader());

    MINTBOY_REQUIRE(cartridge.Title() == "MINTBOY");
    MINTBOY_REQUIRE(cartridge.CartridgeType() == 0x00);
    MINTBOY_REQUIRE(cartridge.RomSizeCode() == 0x00);
    MINTBOY_REQUIRE(cartridge.RamSizeCode() == 0x00);
    MINTBOY_REQUIRE(cartridge.HasValidHeaderChecksum());
}

MINTBOY_TEST(cartridge_returns_ff_for_out_of_range_reads)
{
    const mintboy::Cartridge cartridge(MakeRomWithHeader());

    MINTBOY_REQUIRE(cartridge.Read(0xFFFF) == 0xFF);
}
