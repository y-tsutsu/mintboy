#include "mintboy/cartridge.hpp"

#include <algorithm>
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
    }

    Cartridge::Cartridge(std::vector<Byte> data)
        : data_(std::move(data))
    {
        if (data_.size() < MinimumRomSize)
        {
            throw std::invalid_argument("ROM image is too small to contain a Game Boy header");
        }
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
        if (address >= data_.size())
        {
            return 0xFF;
        }

        return data_[address];
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

    Byte Cartridge::RomSizeCode() const
    {
        return data_[RomSizeAddress];
    }

    Byte Cartridge::RamSizeCode() const
    {
        return data_[RamSizeAddress];
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
}
