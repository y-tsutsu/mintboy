#pragma once

#include "mintboy/types.hpp"

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
        [[nodiscard]] const std::vector<Byte> &Data() const;
        [[nodiscard]] std::string Title() const;
        [[nodiscard]] Byte CartridgeType() const;
        [[nodiscard]] Byte RomSizeCode() const;
        [[nodiscard]] Byte RamSizeCode() const;
        [[nodiscard]] bool HasValidHeaderChecksum() const;

    private:
        std::vector<Byte> data_;
    };
}
