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
    mintboy::Cartridge cartridge(MakeRomWithHeader());

    MINTBOY_REQUIRE(cartridge.Title() == "MINTBOY");
    MINTBOY_REQUIRE(cartridge.CartridgeType() == 0x00);
    MINTBOY_REQUIRE(cartridge.CartridgeTypeName() == "ROM ONLY");
    MINTBOY_REQUIRE(cartridge.RomSizeCode() == 0x00);
    MINTBOY_REQUIRE(cartridge.RomSizeBytes() == 32 * 1024);
    MINTBOY_REQUIRE(cartridge.RomBankCount() == 2);
    MINTBOY_REQUIRE(cartridge.RamSizeCode() == 0x00);
    MINTBOY_REQUIRE(cartridge.RamSizeBytes() == 0);
    MINTBOY_REQUIRE(cartridge.HasValidHeaderChecksum());
}

MINTBOY_TEST(cartridge_returns_ff_for_out_of_range_reads)
{
    mintboy::Cartridge cartridge(MakeRomWithHeader());

    MINTBOY_REQUIRE(cartridge.Read(0xFFFF) == 0xFF);
}

MINTBOY_TEST(cartridge_switches_mbc1_rom_banks)
{
    std::vector<mintboy::Byte> rom(512 * 1024, 0);
    rom[0x0147] = 0x01;
    rom[0x0148] = 0x04;
    rom[0x0149] = 0x00;

    for (std::size_t bank = 0; bank < 32; ++bank)
    {
        rom[(bank * 0x4000) + 0x0100] = static_cast<mintboy::Byte>(bank);
        rom[(bank * 0x4000) + 0x0000] = static_cast<mintboy::Byte>(0x80 | bank);
    }

    mintboy::Cartridge cartridge(std::move(rom));

    MINTBOY_REQUIRE(cartridge.CartridgeTypeName() == "MBC1");
    MINTBOY_REQUIRE(cartridge.RomSizeBytes() == 512 * 1024);
    MINTBOY_REQUIRE(cartridge.RomBankCount() == 32);
    MINTBOY_REQUIRE(cartridge.Read(0x0100) == 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x01);

    cartridge.Write(0x2000, 0x02);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x02);

    cartridge.Write(0x2000, 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x01);
}

MINTBOY_TEST(cartridge_uses_mbc1_external_ram_enable)
{
    std::vector<mintboy::Byte> rom(0x8000, 0);
    rom[0x0147] = 0x02;
    rom[0x0148] = 0x00;
    rom[0x0149] = 0x02;

    mintboy::Cartridge cartridge(std::move(rom));

    MINTBOY_REQUIRE(cartridge.RamSizeBytes() == 8 * 1024);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFF);

    cartridge.WriteRam(0xA000, 0x12);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFF);

    cartridge.Write(0x0000, 0x0A);
    cartridge.WriteRam(0xA000, 0x34);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x34);

    cartridge.Write(0x0000, 0x00);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFF);
}

MINTBOY_TEST(cartridge_switches_mbc1_external_ram_banks)
{
    std::vector<mintboy::Byte> rom(0x8000, 0);
    rom[0x0147] = 0x03;
    rom[0x0148] = 0x00;
    rom[0x0149] = 0x03;

    mintboy::Cartridge cartridge(std::move(rom));

    MINTBOY_REQUIRE(cartridge.RamSizeBytes() == 32 * 1024);

    cartridge.Write(0x0000, 0x0A);
    cartridge.Write(0x6000, 0x01);

    cartridge.Write(0x4000, 0x00);
    cartridge.WriteRam(0xA000, 0x10);

    cartridge.Write(0x4000, 0x01);
    cartridge.WriteRam(0xA000, 0x20);

    cartridge.Write(0x4000, 0x02);
    cartridge.WriteRam(0xA000, 0x30);

    cartridge.Write(0x4000, 0x00);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x10);

    cartridge.Write(0x4000, 0x01);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x20);

    cartridge.Write(0x4000, 0x02);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x30);
}
