#include "test.hpp"

#include "mintboy/cartridge.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
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

    std::filesystem::path TempPath(const std::string &name)
    {
        return std::filesystem::temp_directory_path() / ("mintboy_" + name);
    }

    void WriteBytes(const std::filesystem::path &path, const std::vector<mintboy::Byte> &bytes)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
}

MINTBOY_TEST(cartridge_reads_header_fields)
{
    mintboy::Cartridge cartridge(MakeRomWithHeader());

    MINTBOY_REQUIRE(cartridge.Title() == "MINTBOY");
    MINTBOY_REQUIRE(cartridge.CgbFlag() == 0x00);
    MINTBOY_REQUIRE(cartridge.HardwareCompatibilityName() == "DMG");
    MINTBOY_REQUIRE(!cartridge.SupportsCgb());
    MINTBOY_REQUIRE(!cartridge.RequiresCgb());
    MINTBOY_REQUIRE(cartridge.CartridgeType() == 0x00);
    MINTBOY_REQUIRE(cartridge.CartridgeTypeName() == "ROM ONLY");
    MINTBOY_REQUIRE(cartridge.RomSizeCode() == 0x00);
    MINTBOY_REQUIRE(cartridge.RomSizeBytes() == 32 * 1024);
    MINTBOY_REQUIRE(cartridge.RomBankCount() == 2);
    MINTBOY_REQUIRE(cartridge.RamSizeCode() == 0x00);
    MINTBOY_REQUIRE(cartridge.RamSizeBytes() == 0);
    MINTBOY_REQUIRE(cartridge.HasValidHeaderChecksum());
}

MINTBOY_TEST(cartridge_reads_cgb_compatibility_flag)
{
    std::vector<mintboy::Byte> compatible_rom = MakeRomWithHeader();
    const std::string compatible_title = "CGBCOMPATIBLE!";
    std::copy(compatible_title.begin(), compatible_title.end(), compatible_rom.begin() + 0x0134);
    compatible_rom[0x0143] = 0x80;

    mintboy::Cartridge compatible_cartridge(std::move(compatible_rom));
    MINTBOY_REQUIRE(compatible_cartridge.Title() == "CGBCOMPATIBLE!");
    MINTBOY_REQUIRE(compatible_cartridge.CgbFlag() == 0x80);
    MINTBOY_REQUIRE(compatible_cartridge.HardwareCompatibilityName() == "DMG/CGB");
    MINTBOY_REQUIRE(compatible_cartridge.SupportsCgb());
    MINTBOY_REQUIRE(!compatible_cartridge.RequiresCgb());

    std::vector<mintboy::Byte> only_rom = MakeRomWithHeader();
    const std::string only_title = "CGBONLY";
    std::copy(only_title.begin(), only_title.end(), only_rom.begin() + 0x0134);
    only_rom[0x0143] = 0xC0;

    mintboy::Cartridge only_cartridge(std::move(only_rom));
    MINTBOY_REQUIRE(only_cartridge.Title() == "CGBONLY");
    MINTBOY_REQUIRE(only_cartridge.CgbFlag() == 0xC0);
    MINTBOY_REQUIRE(only_cartridge.HardwareCompatibilityName() == "CGB only");
    MINTBOY_REQUIRE(only_cartridge.SupportsCgb());
    MINTBOY_REQUIRE(only_cartridge.RequiresCgb());
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

MINTBOY_TEST(cartridge_switches_mbc1_large_rom_banks)
{
    std::vector<mintboy::Byte> rom(1024 * 1024, 0);
    rom[0x0147] = 0x01;
    rom[0x0148] = 0x05;
    rom[0x0149] = 0x00;

    for (std::size_t bank = 0; bank < 64; ++bank)
    {
        rom[(bank * 0x4000) + 0x0100] = static_cast<mintboy::Byte>(bank);
    }

    mintboy::Cartridge cartridge(std::move(rom));

    MINTBOY_REQUIRE(cartridge.RomBankCount() == 64);
    MINTBOY_REQUIRE(cartridge.Read(0x0100) == 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x01);

    cartridge.Write(0x4000, 0x01);
    cartridge.Write(0x2000, 0x02);
    MINTBOY_REQUIRE(cartridge.Read(0x0100) == 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x22);

    cartridge.Write(0x6000, 0x01);
    MINTBOY_REQUIRE(cartridge.Read(0x0100) == 0x20);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x02);
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

MINTBOY_TEST(cartridge_switches_mbc2_rom_banks_and_internal_ram)
{
    std::vector<mintboy::Byte> rom(256 * 1024, 0);
    rom[0x0147] = 0x06;
    rom[0x0148] = 0x03;
    rom[0x0149] = 0x00;

    for (std::size_t bank = 0; bank < 16; ++bank)
    {
        rom[(bank * 0x4000) + 0x0100] = static_cast<mintboy::Byte>(bank);
    }

    mintboy::Cartridge cartridge(std::move(rom));

    MINTBOY_REQUIRE(cartridge.CartridgeTypeName() == "MBC2+BATTERY");
    MINTBOY_REQUIRE(cartridge.HasBattery());
    MINTBOY_REQUIRE(cartridge.HasExternalRam());
    MINTBOY_REQUIRE(cartridge.RamSizeBytes() == 0);
    MINTBOY_REQUIRE(cartridge.Read(0x0100) == 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x01);

    cartridge.Write(0x2100, 0x05);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x05);

    cartridge.Write(0x2100, 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x01);

    cartridge.WriteRam(0xA000, 0x12);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFF);

    cartridge.Write(0x0000, 0x0A);
    cartridge.WriteRam(0xA000, 0x2C);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFC);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA200) == 0xFC);

    cartridge.WriteRam(0xA1FF, 0x07);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA1FF) == 0xF7);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA3FF) == 0xF7);

    cartridge.Write(0x0100, 0x00);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFC);

    cartridge.Write(0x0200, 0x00);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFF);
}

MINTBOY_TEST(cartridge_switches_mbc3_rom_and_ram_banks)
{
    std::vector<mintboy::Byte> rom(512 * 1024, 0);
    rom[0x0147] = 0x13;
    rom[0x0148] = 0x04;
    rom[0x0149] = 0x03;

    for (std::size_t bank = 0; bank < 32; ++bank)
    {
        rom[(bank * 0x4000) + 0x0100] = static_cast<mintboy::Byte>(bank);
    }

    mintboy::Cartridge cartridge(std::move(rom));

    MINTBOY_REQUIRE(cartridge.CartridgeTypeName() == "MBC3+RAM+BATTERY");
    MINTBOY_REQUIRE(cartridge.HasBattery());
    MINTBOY_REQUIRE(cartridge.Read(0x0100) == 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x01);

    cartridge.Write(0x2000, 0x05);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x05);

    cartridge.Write(0x0000, 0x0A);
    cartridge.Write(0x4000, 0x00);
    cartridge.WriteRam(0xA000, 0x10);
    cartridge.Write(0x4000, 0x01);
    cartridge.WriteRam(0xA000, 0x20);
    cartridge.Write(0x4000, 0x08);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0xFF);

    cartridge.Write(0x4000, 0x00);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x10);
    cartridge.Write(0x4000, 0x01);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x20);
}

MINTBOY_TEST(cartridge_switches_mbc5_rom_and_ram_banks)
{
    std::vector<mintboy::Byte> rom(8 * 1024 * 1024, 0);
    rom[0x0147] = 0x1B;
    rom[0x0148] = 0x08;
    rom[0x0149] = 0x03;

    for (std::size_t bank = 0; bank < 512; ++bank)
    {
        rom[(bank * 0x4000) + 0x0100] = static_cast<mintboy::Byte>(bank & 0xFF);
        rom[(bank * 0x4000) + 0x0101] = static_cast<mintboy::Byte>((bank >> 8) & 0x01);
    }

    mintboy::Cartridge cartridge(std::move(rom));

    MINTBOY_REQUIRE(cartridge.CartridgeTypeName() == "MBC5+RAM+BATTERY");
    MINTBOY_REQUIRE(cartridge.HasBattery());
    MINTBOY_REQUIRE(cartridge.RomBankCount() == 512);
    MINTBOY_REQUIRE(cartridge.Read(0x0100) == 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x01);
    MINTBOY_REQUIRE(cartridge.Read(0x4101) == 0x00);

    cartridge.Write(0x2000, 0x34);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x34);
    MINTBOY_REQUIRE(cartridge.Read(0x4101) == 0x00);

    cartridge.Write(0x3000, 0x01);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x34);
    MINTBOY_REQUIRE(cartridge.Read(0x4101) == 0x01);

    cartridge.Write(0x2000, 0x00);
    cartridge.Write(0x3000, 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4100) == 0x00);
    MINTBOY_REQUIRE(cartridge.Read(0x4101) == 0x00);

    cartridge.Write(0x0000, 0x0A);
    cartridge.Write(0x4000, 0x00);
    cartridge.WriteRam(0xA000, 0x10);
    cartridge.Write(0x4000, 0x01);
    cartridge.WriteRam(0xA000, 0x20);
    cartridge.Write(0x4000, 0x03);
    cartridge.WriteRam(0xA000, 0x40);

    cartridge.Write(0x4000, 0x00);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x10);
    cartridge.Write(0x4000, 0x01);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x20);
    cartridge.Write(0x4000, 0x03);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x40);
}

MINTBOY_TEST(cartridge_loads_and_saves_battery_ram)
{
    const std::filesystem::path save_path = TempPath("battery_ram.sav");
    std::filesystem::remove(save_path);

    std::vector<mintboy::Byte> rom(0x8000, 0);
    rom[0x0147] = 0x03;
    rom[0x0148] = 0x00;
    rom[0x0149] = 0x02;

    mintboy::Cartridge cartridge(std::move(rom));
    cartridge.Write(0x0000, 0x0A);
    cartridge.WriteRam(0xA000, 0x42);
    MINTBOY_REQUIRE(cartridge.IsRamDirty());

    cartridge.SaveRamToFile(save_path);

    std::vector<mintboy::Byte> reloaded_rom(0x8000, 0);
    reloaded_rom[0x0147] = 0x03;
    reloaded_rom[0x0148] = 0x00;
    reloaded_rom[0x0149] = 0x02;
    mintboy::Cartridge reloaded(std::move(reloaded_rom));
    reloaded.LoadRamFromFile(save_path);
    reloaded.Write(0x0000, 0x0A);

    MINTBOY_REQUIRE(reloaded.ReadRam(0xA000) == 0x42);

    std::filesystem::remove(save_path);
}

MINTBOY_TEST(cartridge_loads_and_saves_mbc2_internal_ram)
{
    const std::filesystem::path save_path = TempPath("mbc2_ram.sav");
    std::filesystem::remove(save_path);

    std::vector<mintboy::Byte> rom(256 * 1024, 0);
    rom[0x0147] = 0x06;
    rom[0x0148] = 0x03;
    rom[0x0149] = 0x00;

    mintboy::Cartridge cartridge(std::move(rom));
    cartridge.Write(0x0000, 0x0A);
    cartridge.WriteRam(0xA123, 0x4B);
    MINTBOY_REQUIRE(cartridge.IsRamDirty());

    cartridge.SaveRamToFile(save_path);

    std::vector<mintboy::Byte> reloaded_rom(256 * 1024, 0);
    reloaded_rom[0x0147] = 0x06;
    reloaded_rom[0x0148] = 0x03;
    reloaded_rom[0x0149] = 0x00;
    mintboy::Cartridge reloaded(std::move(reloaded_rom));
    reloaded.LoadRamFromFile(save_path);
    reloaded.Write(0x0000, 0x0A);

    MINTBOY_REQUIRE(reloaded.ReadRam(0xA123) == 0xFB);
    MINTBOY_REQUIRE(std::filesystem::file_size(save_path) == 512);

    std::filesystem::remove(save_path);
}

MINTBOY_TEST(cartridge_load_from_file_uses_sav_next_to_rom)
{
    const std::filesystem::path rom_path = TempPath("autoload.gb");
    const std::filesystem::path save_path = TempPath("autoload.sav");
    std::filesystem::remove(rom_path);
    std::filesystem::remove(save_path);

    std::vector<mintboy::Byte> rom(0x8000, 0);
    rom[0x0147] = 0x03;
    rom[0x0148] = 0x00;
    rom[0x0149] = 0x02;
    WriteBytes(rom_path, rom);

    std::vector<mintboy::Byte> save(8 * 1024, 0);
    save[0] = 0x77;
    WriteBytes(save_path, save);

    mintboy::Cartridge cartridge = mintboy::Cartridge::LoadFromFile(rom_path);
    cartridge.Write(0x0000, 0x0A);
    MINTBOY_REQUIRE(cartridge.ReadRam(0xA000) == 0x77);

    cartridge.WriteRam(0xA000, 0x88);
    cartridge.SaveRam();

    mintboy::Cartridge reloaded = mintboy::Cartridge::LoadFromFile(rom_path);
    reloaded.Write(0x0000, 0x0A);
    MINTBOY_REQUIRE(reloaded.ReadRam(0xA000) == 0x88);

    std::filesystem::remove(rom_path);
    std::filesystem::remove(save_path);
}
