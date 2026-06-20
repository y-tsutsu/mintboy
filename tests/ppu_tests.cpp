#include "test.hpp"

#include "mintboy/cartridge.hpp"
#include "mintboy/memory.hpp"

#include <vector>

namespace
{
    mintboy::Cartridge MakeCartridge()
    {
        return mintboy::Cartridge(std::vector<mintboy::Byte>(0x8000, 0));
    }
}

MINTBOY_TEST(ppu_lcd_enable_initializes_mode_and_ly)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF44, 0x80);
    memory.WriteByte(0xFF40, 0x80);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 0);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);
}

MINTBOY_TEST(ppu_starts_in_post_boot_lcd_state)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF40) == 0x91);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF47) == 0xFC);
}

MINTBOY_TEST(ppu_updates_mode_during_visible_scanline)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x80);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);

    memory.Tick(80);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 3);

    memory.Tick(172);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 0);

    memory.Tick(204);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 1);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);
}

MINTBOY_TEST(ppu_enters_vblank_and_requests_interrupt)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x80);
    memory.Tick(456 * 144);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 144);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 1);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x01) != 0);
}

MINTBOY_TEST(ppu_wraps_ly_after_vblank)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x80);
    memory.Tick(456 * 154);

    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 0);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 2);
}

MINTBOY_TEST(ppu_sets_lyc_compare_flag)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF45, 1);
    memory.WriteByte(0xFF40, 0x80);

    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x04) == 0);

    memory.Tick(456);
    MINTBOY_REQUIRE(memory.ReadByte(0xFF44) == 1);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x04) != 0);
}

MINTBOY_TEST(ppu_requests_stat_interrupt_on_lyc_match)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF41, 0x40);
    memory.WriteByte(0xFF45, 1);
    memory.WriteByte(0xFF40, 0x80);
    memory.Tick(456);

    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x04) != 0);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x02) != 0);
}

MINTBOY_TEST(ppu_requests_stat_interrupt_on_mode_transitions)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x00);
    memory.WriteByte(0xFF41, 0x38);
    memory.WriteByte(0xFF40, 0x80);

    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x02) != 0);

    memory.WriteByte(0xFF0F, 0);
    memory.Tick(80);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x02) == 0);

    memory.Tick(172);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 0);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x02) != 0);

    memory.WriteByte(0xFF0F, 0);
    memory.Tick((456 * 143) + 204);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF41) & 0x03) == 1);
    MINTBOY_REQUIRE((memory.ReadByte(0xFF0F) & 0x02) != 0);
}

MINTBOY_TEST(ppu_renders_placeholder_scanline_into_framebuffer)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x80);
    memory.Tick(456);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] != 0);
    MINTBOY_REQUIRE(framebuffer[0] == framebuffer[24]);
}

MINTBOY_TEST(ppu_renders_background_tiles_from_vram)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0x8000, 0xFF);
    memory.WriteByte(0x8001, 0x00);
    memory.WriteByte(0x8002, 0xFF);
    memory.WriteByte(0x8003, 0x00);
    memory.WriteByte(0x9800, 0x00);
    memory.WriteByte(0x9801, 0x01);
    memory.WriteByte(0xFF47, 0xE4);
    memory.WriteByte(0xFF40, 0x91);

    memory.Tick(456);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] == 0xFF8BAC0F);
    MINTBOY_REQUIRE(framebuffer[7] == 0xFF8BAC0F);
    MINTBOY_REQUIRE(framebuffer[8] == 0xFF9BBC0F);
}

MINTBOY_TEST(ppu_latches_scroll_before_hblank)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x00);
    memory.WriteByte(0x8000, 0xFF);
    memory.WriteByte(0x8001, 0x00);
    memory.WriteByte(0x8010, 0x00);
    memory.WriteByte(0x8011, 0x00);
    memory.WriteByte(0x9800, 0x00);
    memory.WriteByte(0x9801, 0x01);
    memory.WriteByte(0xFF47, 0xE4);
    memory.WriteByte(0xFF43, 0);
    memory.WriteByte(0xFF40, 0x91);

    memory.Tick(248);
    memory.WriteByte(0xFF43, 8);
    memory.Tick(208);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] == 0xFF8BAC0F);

    memory.Tick(248);
    MINTBOY_REQUIRE(framebuffer[mintboy::Memory::ScreenWidth] == 0xFF9BBC0F);
}

MINTBOY_TEST(ppu_latches_scroll_at_scanline_start)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x00);
    memory.WriteByte(0x8000, 0xFF);
    memory.WriteByte(0x8001, 0x00);
    memory.WriteByte(0x8010, 0x00);
    memory.WriteByte(0x8011, 0x00);
    memory.WriteByte(0x9800, 0x00);
    memory.WriteByte(0x9801, 0x01);
    memory.WriteByte(0xFF47, 0xE4);
    memory.WriteByte(0xFF43, 0);
    memory.WriteByte(0xFF40, 0x91);

    memory.Tick(80);
    memory.WriteByte(0xFF43, 8);
    memory.Tick(376);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] == 0xFF8BAC0F);

    memory.Tick(456);
    MINTBOY_REQUIRE(framebuffer[mintboy::Memory::ScreenWidth] == 0xFF9BBC0F);
}

MINTBOY_TEST(ppu_renders_window_tiles_over_background)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0x8000, 0x00);
    memory.WriteByte(0x8001, 0x00);
    memory.WriteByte(0x8010, 0xFF);
    memory.WriteByte(0x8011, 0x00);
    memory.WriteByte(0x8012, 0xFF);
    memory.WriteByte(0x8013, 0x00);
    memory.WriteByte(0x9800, 0x00);
    memory.WriteByte(0x9C00, 0x01);
    memory.WriteByte(0x9C01, 0x01);
    memory.WriteByte(0xFF47, 0xE4);
    memory.WriteByte(0xFF4A, 0);
    memory.WriteByte(0xFF4B, 7);
    memory.WriteByte(0xFF40, 0xF1);

    memory.Tick(456);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] == 0xFF8BAC0F);
    MINTBOY_REQUIRE(framebuffer[8] == 0xFF8BAC0F);
}

MINTBOY_TEST(ppu_does_not_render_window_before_wy_or_wx)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0x8000, 0x00);
    memory.WriteByte(0x8001, 0x00);
    memory.WriteByte(0x8010, 0xFF);
    memory.WriteByte(0x8011, 0x00);
    memory.WriteByte(0x9800, 0x00);
    memory.WriteByte(0x9C00, 0x01);
    memory.WriteByte(0xFF47, 0xE4);
    memory.WriteByte(0xFF4A, 2);
    memory.WriteByte(0xFF4B, 15);
    memory.WriteByte(0xFF40, 0xF1);

    memory.Tick(456);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] == 0xFF9BBC0F);
    MINTBOY_REQUIRE(framebuffer[8] == 0xFF9BBC0F);

    memory.Tick(456);
    MINTBOY_REQUIRE(framebuffer[mintboy::Memory::ScreenWidth + 8] == 0xFF9BBC0F);

    memory.Tick(456);
    MINTBOY_REQUIRE(framebuffer[mintboy::Memory::ScreenWidth * 2 + 7] == 0xFF9BBC0F);
    MINTBOY_REQUIRE(framebuffer[mintboy::Memory::ScreenWidth * 2 + 8] == 0xFF8BAC0F);
}

MINTBOY_TEST(ppu_oam_dma_copies_160_bytes)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x00);
    memory.WriteByte(0xC000, 0x11);
    memory.WriteByte(0xC001, 0x22);
    memory.WriteByte(0xC09F, 0x33);

    memory.WriteByte(0xFF46, 0xC0);

    MINTBOY_REQUIRE(memory.ReadByte(0xFE00) == 0x11);
    MINTBOY_REQUIRE(memory.ReadByte(0xFE01) == 0x22);
    MINTBOY_REQUIRE(memory.ReadByte(0xFE9F) == 0x33);
}

MINTBOY_TEST(ppu_renders_sprite_pixels_from_oam)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x00);
    memory.WriteByte(0x8000, 0x80);
    memory.WriteByte(0x8001, 0x00);
    memory.WriteByte(0xFE00, 16);
    memory.WriteByte(0xFE01, 8);
    memory.WriteByte(0xFE02, 0);
    memory.WriteByte(0xFE03, 0);
    memory.WriteByte(0xFF48, 0xE4);
    memory.WriteByte(0xFF40, 0x82);

    memory.Tick(456);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] == 0xFF8BAC0F);
    MINTBOY_REQUIRE(framebuffer[1] == 0xFF9BBC0F);
}

MINTBOY_TEST(ppu_respects_sprite_background_priority)
{
    mintboy::Cartridge cartridge = MakeCartridge();
    mintboy::Memory memory(cartridge);

    memory.WriteByte(0xFF40, 0x00);
    memory.WriteByte(0x8000, 0xFF);
    memory.WriteByte(0x8001, 0x00);
    memory.WriteByte(0x8002, 0xFF);
    memory.WriteByte(0x8003, 0x00);
    memory.WriteByte(0x8010, 0x80);
    memory.WriteByte(0x8011, 0x80);
    memory.WriteByte(0x9800, 0);
    memory.WriteByte(0xFE00, 16);
    memory.WriteByte(0xFE01, 8);
    memory.WriteByte(0xFE02, 1);
    memory.WriteByte(0xFE03, 0x80);
    memory.WriteByte(0xFF47, 0xE4);
    memory.WriteByte(0xFF48, 0xE4);
    memory.WriteByte(0xFF40, 0x93);

    memory.Tick(456);

    const auto &framebuffer = memory.GetFramebuffer();
    MINTBOY_REQUIRE(framebuffer[0] == 0xFF8BAC0F);
}
