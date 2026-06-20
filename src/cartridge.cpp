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
        constexpr std::size_t CgbFlagAddress = 0x0143;
        constexpr std::size_t CartridgeTypeAddress = 0x0147;
        constexpr std::size_t RomSizeAddress = 0x0148;
        constexpr std::size_t RamSizeAddress = 0x0149;
        constexpr std::size_t HeaderChecksumAddress = 0x014D;
        constexpr std::size_t MinimumRomSize = 0x0150;
        constexpr std::size_t Mbc2RamSize = 512;

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

        external_ram_.resize(IsMbc2() ? Mbc2RamSize : RamSizeBytes(), 0);
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

        Cartridge cartridge(std::move(data));
        if (cartridge.HasBattery() && cartridge.HasExternalRam())
        {
            std::filesystem::path save_path = path;
            save_path.replace_extension(".sav");
            cartridge.save_path_ = save_path;
            cartridge.LoadRamFromFile(save_path);
        }
        return cartridge;
    }

    Byte Cartridge::Read(Word address) const
    {
        std::size_t resolved_address = address;

        if ((IsMbc1() || IsMbc2() || IsMbc3() || IsMbc5()) && address <= 0x3FFF)
        {
            resolved_address = (SelectedFixedRomBank() * 0x4000) + address;
        }
        else if ((IsMbc1() || IsMbc2() || IsMbc3() || IsMbc5()) && address >= 0x4000 && address <= 0x7FFF)
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

        const std::size_t resolved_address = IsMbc2()
                                                 ? static_cast<std::size_t>(address & 0x01FF)
                                                 : (SelectedRamBank() * 0x2000) + (address - 0xA000);
        if (resolved_address >= external_ram_.size())
        {
            return 0xFF;
        }

        return IsMbc2() ? static_cast<Byte>(0xF0 | (external_ram_[resolved_address] & 0x0F)) : external_ram_[resolved_address];
    }

    void Cartridge::Write(Word address, Byte value)
    {
        if ((!IsMbc1() && !IsMbc2() && !IsMbc3() && !IsMbc5()) || address > 0x7FFF)
        {
            return;
        }

        if (IsMbc2())
        {
            if (address <= 0x3FFF)
            {
                if ((address & 0x0100) == 0)
                {
                    external_ram_enabled_ = (value & 0x0F) == 0x0A;
                    return;
                }

                mbc2_rom_bank_ = static_cast<Byte>(value & 0x0F);
                if (mbc2_rom_bank_ == 0)
                {
                    mbc2_rom_bank_ = 1;
                }
            }
            return;
        }

        if (address <= 0x1FFF)
        {
            external_ram_enabled_ = (value & 0x0F) == 0x0A;
            return;
        }

        if (address >= 0x2000 && address <= 0x3FFF)
        {
            if (IsMbc1())
            {
                mbc1_rom_bank_low_ = static_cast<Byte>(value & 0x1F);
                if (mbc1_rom_bank_low_ == 0)
                {
                    mbc1_rom_bank_low_ = 1;
                }
            }
            else if (IsMbc3())
            {
                mbc3_rom_bank_ = static_cast<Byte>(value & 0x7F);
                if (mbc3_rom_bank_ == 0)
                {
                    mbc3_rom_bank_ = 1;
                }
            }
            else if (address <= 0x2FFF)
            {
                mbc5_rom_bank_ = static_cast<Word>((mbc5_rom_bank_ & 0x100) | value);
            }
            else
            {
                mbc5_rom_bank_ = static_cast<Word>((mbc5_rom_bank_ & 0x0FF) | ((value & 0x01) << 8));
            }
            return;
        }

        if (address >= 0x4000 && address <= 0x5FFF)
        {
            if (IsMbc1())
            {
                mbc1_bank_high_ = static_cast<Byte>(value & 0x03);
            }
            else if (IsMbc3())
            {
                mbc3_ram_bank_ = static_cast<Byte>(value & 0x0F);
            }
            else
            {
                mbc5_ram_bank_ = static_cast<Byte>(value & 0x0F);
            }
            return;
        }

        if (address >= 0x6000 && address <= 0x7FFF)
        {
            if (IsMbc1())
            {
                mbc1_banking_mode_ = static_cast<Byte>(value & 0x01);
            }
        }
    }

    void Cartridge::WriteRam(Word address, Byte value)
    {
        if (!external_ram_enabled_ || external_ram_.empty() || address < 0xA000 || address > 0xBFFF)
        {
            return;
        }

        const std::size_t resolved_address = IsMbc2()
                                                 ? static_cast<std::size_t>(address & 0x01FF)
                                                 : (SelectedRamBank() * 0x2000) + (address - 0xA000);
        if (resolved_address >= external_ram_.size())
        {
            return;
        }

        external_ram_[resolved_address] = IsMbc2() ? static_cast<Byte>(value & 0x0F) : value;
        external_ram_dirty_ = true;
    }

    void Cartridge::LoadRamFromFile(const std::filesystem::path &path)
    {
        if (external_ram_.empty())
        {
            return;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return;
        }

        file.read(reinterpret_cast<char *>(external_ram_.data()), static_cast<std::streamsize>(external_ram_.size()));
        external_ram_dirty_ = false;
    }

    void Cartridge::SaveRamToFile(const std::filesystem::path &path) const
    {
        if (external_ram_.empty() || !HasBattery())
        {
            return;
        }

        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("failed to open save RAM: " + path.string());
        }

        file.write(reinterpret_cast<const char *>(external_ram_.data()), static_cast<std::streamsize>(external_ram_.size()));
        if (!file)
        {
            throw std::runtime_error("failed to write save RAM: " + path.string());
        }
    }

    void Cartridge::SaveRam()
    {
        if (!save_path_.has_value() || !external_ram_dirty_)
        {
            return;
        }

        SaveRamToFile(*save_path_);
        external_ram_dirty_ = false;
    }

    const std::vector<Byte> &Cartridge::Data() const
    {
        return data_;
    }

    std::string Cartridge::Title() const
    {
        const auto begin = data_.begin() + HeaderTitleBegin;
        const auto end = data_.begin() + (SupportsCgb() ? CgbFlagAddress : HeaderTitleEnd + 1);
        const auto terminator = std::find(begin, end, 0);
        return std::string(begin, terminator);
    }

    Byte Cartridge::CgbFlag() const
    {
        return data_[CgbFlagAddress];
    }

    std::string Cartridge::HardwareCompatibilityName() const
    {
        if (RequiresCgb())
        {
            return "CGB only";
        }
        if (SupportsCgb())
        {
            return "DMG/CGB";
        }
        return "DMG";
    }

    bool Cartridge::SupportsCgb() const
    {
        return CgbFlag() == 0x80 || CgbFlag() == 0xC0;
    }

    bool Cartridge::RequiresCgb() const
    {
        return CgbFlag() == 0xC0;
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
        case 0x05:
            return "MBC2";
        case 0x06:
            return "MBC2+BATTERY";
        case 0x0F:
            return "MBC3+TIMER+BATTERY";
        case 0x10:
            return "MBC3+TIMER+RAM+BATTERY";
        case 0x11:
            return "MBC3";
        case 0x12:
            return "MBC3+RAM";
        case 0x13:
            return "MBC3+RAM+BATTERY";
        case 0x19:
            return "MBC5";
        case 0x1A:
            return "MBC5+RAM";
        case 0x1B:
            return "MBC5+RAM+BATTERY";
        case 0x1C:
            return "MBC5+RUMBLE";
        case 0x1D:
            return "MBC5+RUMBLE+RAM";
        case 0x1E:
            return "MBC5+RUMBLE+RAM+BATTERY";
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

    bool Cartridge::HasBattery() const
    {
        switch (CartridgeType())
        {
        case 0x03:
        case 0x06:
        case 0x0F:
        case 0x10:
        case 0x13:
        case 0x1B:
        case 0x1E:
            return true;
        default:
            return false;
        }
    }

    bool Cartridge::HasExternalRam() const
    {
        return !external_ram_.empty();
    }

    bool Cartridge::IsRamDirty() const
    {
        return external_ram_dirty_;
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

    bool Cartridge::IsMbc2() const
    {
        const Byte type = CartridgeType();
        return type == 0x05 || type == 0x06;
    }

    bool Cartridge::IsMbc3() const
    {
        const Byte type = CartridgeType();
        return type == 0x0F || type == 0x10 || type == 0x11 || type == 0x12 || type == 0x13;
    }

    bool Cartridge::IsMbc5() const
    {
        const Byte type = CartridgeType();
        return type >= 0x19 && type <= 0x1E;
    }

    std::size_t Cartridge::SelectedFixedRomBank() const
    {
        if (IsMbc2() || IsMbc3() || IsMbc5())
        {
            return 0;
        }

        if (mbc1_banking_mode_ == 0)
        {
            return 0;
        }

        const std::size_t bank_count = RomBankCount();
        if (bank_count == 0)
        {
            return 0;
        }

        return (static_cast<std::size_t>(mbc1_bank_high_) << 5) % bank_count;
    }

    std::size_t Cartridge::SelectedRomBank() const
    {
        if (IsMbc3())
        {
            const std::size_t bank_count = RomBankCount();
            if (bank_count == 0)
            {
                return 1;
            }
            const std::size_t bank = mbc3_rom_bank_ % bank_count;
            return bank == 0 ? 1 : bank;
        }

        if (IsMbc2())
        {
            const std::size_t bank_count = RomBankCount();
            if (bank_count == 0)
            {
                return 1;
            }
            const std::size_t bank = mbc2_rom_bank_ % bank_count;
            return bank == 0 ? 1 : bank;
        }

        if (IsMbc5())
        {
            const std::size_t bank_count = RomBankCount();
            if (bank_count == 0)
            {
                return 0;
            }
            return mbc5_rom_bank_ % bank_count;
        }

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
        if (IsMbc3())
        {
            if (mbc3_ram_bank_ > 0x03)
            {
                return external_ram_.empty() ? 0 : external_ram_.size() / 0x2000;
            }
            return mbc3_ram_bank_;
        }

        if (IsMbc5())
        {
            const std::size_t bank_count = external_ram_.size() / 0x2000;
            if (bank_count == 0)
            {
                return 0;
            }
            return mbc5_ram_bank_ % bank_count;
        }

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
