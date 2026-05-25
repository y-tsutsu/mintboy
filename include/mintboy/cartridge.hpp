#pragma once

#include "mintboy/types.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace mintboy
{
    class Cartridge
    {
    public:
        explicit Cartridge(std::vector<Byte> data);

        static Cartridge LoadFromFile(const std::filesystem::path &path);

        [[nodiscard]] Byte Read(Word address) const;
        [[nodiscard]] Byte ReadRam(Word address) const;
        void Write(Word address, Byte value);
        void WriteRam(Word address, Byte value);

        [[nodiscard]] const std::vector<Byte> &Data() const;
        [[nodiscard]] std::string Title() const;
        [[nodiscard]] Byte CartridgeType() const;
        [[nodiscard]] std::string CartridgeTypeName() const;
        [[nodiscard]] Byte RomSizeCode() const;
        [[nodiscard]] std::size_t RomSizeBytes() const;
        [[nodiscard]] std::size_t RomBankCount() const;
        [[nodiscard]] Byte RamSizeCode() const;
        [[nodiscard]] std::size_t RamSizeBytes() const;
        [[nodiscard]] bool HasValidHeaderChecksum() const;

    private:
        [[nodiscard]] bool IsMbc1() const;
        [[nodiscard]] std::size_t SelectedFixedRomBank() const;
        [[nodiscard]] std::size_t SelectedRomBank() const;
        [[nodiscard]] std::size_t SelectedRamBank() const;

        std::vector<Byte> data_;
        std::vector<Byte> external_ram_;
        Byte mbc1_rom_bank_low_ = 1;
        Byte mbc1_bank_high_ = 0;
        Byte mbc1_banking_mode_ = 0;
        bool external_ram_enabled_ = false;
    };
}
