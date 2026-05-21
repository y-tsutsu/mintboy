#include "mintboy/cartridge.hpp"

#include <exception>
#include <iomanip>
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: mintboy <rom.gb>\n";
        return 2;
    }

    try
    {
        const mintboy::Cartridge cartridge = mintboy::Cartridge::LoadFromFile(argv[1]);
        std::cout << "Title: " << cartridge.Title() << '\n';
        std::cout << "Cartridge type: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cartridge.CartridgeType())
                  << " (" << cartridge.CartridgeTypeName() << ")\n";
        std::cout << "ROM size code: 0x" << std::hex << std::setw(2) << static_cast<int>(cartridge.RomSizeCode())
                  << std::dec << " (" << cartridge.RomSizeBytes() / 1024 << " KiB, " << cartridge.RomBankCount() << " banks)\n";
        std::cout << "RAM size code: 0x" << std::hex << std::setw(2) << static_cast<int>(cartridge.RamSizeCode())
                  << std::dec << " (" << cartridge.RamSizeBytes() / 1024 << " KiB)\n";
        std::cout << "Header checksum: " << (cartridge.HasValidHeaderChecksum() ? "ok" : "invalid") << '\n';
    }
    catch (const std::exception &error)
    {
        std::cerr << "mintboy: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
