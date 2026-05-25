#include "mintboy/cartridge.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace mintboy
{
    namespace
    {
        constexpr std::size_t HeaderTitleBegin = 0x0134;
        constexpr std::size_t HeaderTitleEnd = 0x0143;
        constexpr std::size_t CartridgeTypeAddress = 0x0147;
        constexpr std::size_t RomSizeAddress = 0x0148;
        constexpr std::size_t RamSizeAddress = 0x0149;
        constexpr std::size_t HeaderChecksumAddress = 0x014D;
        constexpr std::size_t MinimumRomSize = 0x0150;

        constexpr std::array<std::size_t, 9> RomSizeTable = {
            32 * 1024,
            64 * 1024,
            128 * 1024,
            256 * 1024,
            512 * 1024,
            1024 * 1024,
            2 * 1024 * 1024,
            4 * 1024 * 1024,
            8 * 1024 * 1024,
        };
    }

    Cartridge::Cartridge(std::vector<Byte> data)
        : data_(std::move(data))
    {
        if (data_.size() < MinimumRomSize)
        {
            throw std::invalid_argument("ROM image is too small to contain a Game Boy header");
        }

        external_ram_.resize(RamSizeBytes(), 0);
    }

    Cartridge Cartridge::LoadFromFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("failed to open ROM: " + path.string());
        }

        std::vector<Byte> data{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};

        return Cartridge(std::move(data));
    }

    Byte Cartridge::Read(Word address) const
    {
        std::size_t resolved_address = address;

        if (IsMbc1() && address >= 0x4000 && address <= 0x7FFF)
        {
            resolved_address = (SelectedRomBank() * 0x4000) + (address - 0x4000);
        }

        if (resolved_address >= data_.size())
        {
            return 0xFF;
        }

        return data_[resolved_address];
    }

    Byte Cartridge::ReadRam(Word address) const
    {
        if (!external_ram_enabled_ || external_ram_.empty() || address < 0xA000 || address > 0xBFFF)
        {
            return 0xFF;
        }

        const std::size_t resolved_address = (SelectedRamBank() * 0x2000) + (address - 0xA000);
        if (resolved_address >= external_ram_.size())
        {
            return 0xFF;
        }

        return external_ram_[resolved_address];
    }

    void Cartridge::Write(Word address, Byte value)
    {
        if (!IsMbc1() || address > 0x7FFF)
        {
            return;
        }

        if (address <= 0x1FFF)
        {
            external_ram_enabled_ = (value & 0x0F) == 0x0A;
            return;
        }

        if (address >= 0x2000 && address <= 0x3FFF)
        {
            mbc1_rom_bank_low_ = static_cast<Byte>(value & 0x1F);
            if (mbc1_rom_bank_low_ == 0)
            {
                mbc1_rom_bank_low_ = 1;
            }
            return;
        }

        if (address >= 0x4000 && address <= 0x5FFF)
        {
            mbc1_bank_high_ = static_cast<Byte>(value & 0x03);
            return;
        }

        if (address >= 0x6000 && address <= 0x7FFF)
        {
            mbc1_banking_mode_ = static_cast<Byte>(value & 0x01);
        }
    }

    void Cartridge::WriteRam(Word address, Byte value)
    {
        if (!external_ram_enabled_ || external_ram_.empty() || address < 0xA000 || address > 0xBFFF)
        {
            return;
        }

        const std::size_t resolved_address = (SelectedRamBank() * 0x2000) + (address - 0xA000);
        if (resolved_address >= external_ram_.size())
        {
            return;
        }

        external_ram_[resolved_address] = value;
    }

    const std::vector<Byte> &Cartridge::Data() const
    {
        return data_;
    }

    std::string Cartridge::Title() const
    {
        const auto begin = data_.begin() + HeaderTitleBegin;
        const auto end = data_.begin() + HeaderTitleEnd + 1;
        const auto terminator = std::find(begin, end, 0);
        return std::string(begin, terminator);
    }

    Byte Cartridge::CartridgeType() const
    {
        return data_[CartridgeTypeAddress];
    }

    std::string Cartridge::CartridgeTypeName() const
    {
        switch (CartridgeType())
        {
        case 0x00:
            return "ROM ONLY";
        case 0x01:
            return "MBC1";
        case 0x02:
            return "MBC1+RAM";
        case 0x03:
            return "MBC1+RAM+BATTERY";
        default:
            return "UNKNOWN";
        }
    }

    Byte Cartridge::RomSizeCode() const
    {
        return data_[RomSizeAddress];
    }

    std::size_t Cartridge::RomSizeBytes() const
    {
        const Byte code = RomSizeCode();
        if (code < RomSizeTable.size())
        {
            return RomSizeTable[code];
        }

        return data_.size();
    }

    std::size_t Cartridge::RomBankCount() const
    {
        return RomSizeBytes() / 0x4000;
    }

    Byte Cartridge::RamSizeCode() const
    {
        return data_[RamSizeAddress];
    }

    std::size_t Cartridge::RamSizeBytes() const
    {
        switch (RamSizeCode())
        {
        case 0x00:
            return 0;
        case 0x02:
            return 8 * 1024;
        case 0x03:
            return 32 * 1024;
        case 0x04:
            return 128 * 1024;
        case 0x05:
            return 64 * 1024;
        default:
            return 0;
        }
    }

    bool Cartridge::HasValidHeaderChecksum() const
    {
        Byte checksum = 0;
        for (std::size_t address = 0x0134; address <= 0x014C; ++address)
        {
            checksum = static_cast<Byte>(checksum - data_[address] - 1);
        }

        return checksum == data_[HeaderChecksumAddress];
    }

    bool Cartridge::IsMbc1() const
    {
        const Byte type = CartridgeType();
        return type == 0x01 || type == 0x02 || type == 0x03;
    }

    std::size_t Cartridge::SelectedRomBank() const
    {
        std::size_t bank = mbc1_rom_bank_low_;
        if (mbc1_banking_mode_ == 0)
        {
            bank |= static_cast<std::size_t>(mbc1_bank_high_) << 5;
        }

        const std::size_t bank_count = RomBankCount();
        if (bank_count == 0)
        {
            return 1;
        }

        bank %= bank_count;
        if (bank == 0)
        {
            return 1;
        }

        return bank;
    }

    std::size_t Cartridge::SelectedRamBank() const
    {
        if (!IsMbc1() || mbc1_banking_mode_ == 0)
        {
            return 0;
        }

        const std::size_t bank_count = external_ram_.size() / 0x2000;
        if (bank_count == 0)
        {
            return 0;
        }

        return mbc1_bank_high_ % bank_count;
    }
}
