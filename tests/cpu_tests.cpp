#include "test.hpp"

#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <algorithm>
#include <vector>

namespace
{
    mintboy::Cartridge MakeRom(std::initializer_list<mintboy::Byte> program)
    {
        std::vector<mintboy::Byte> rom(0x8000, 0);
        std::copy(program.begin(), program.end(), rom.begin() + 0x0100);
        return mintboy::Cartridge(std::move(rom));
    }
}

MINTBOY_TEST(cpu_executes_immediate_loads)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x42,
        0x06,
        0x24,
        0x76,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0102);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().b == 0x24);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0104);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.IsHalted());
}

MINTBOY_TEST(cpu_executes_absolute_jump)
{
    mintboy::Cartridge cartridge = MakeRom({
        0xC3,
        0x34,
        0x12,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x1234);
}

MINTBOY_TEST(cpu_loads_16bit_register_pairs)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x01,
        0x34,
        0x12,
        0x11,
        0x78,
        0x56,
        0x21,
        0xBC,
        0x9A,
        0x31,
        0x00,
        0xC0,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().BC() == 0x1234);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().DE() == 0x5678);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0x9ABC);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xC000);
}

MINTBOY_TEST(cpu_reads_and_writes_via_hl)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x21,
        0x00,
        0xC0,
        0x3E,
        0x99,
        0x22,
        0x36,
        0x42,
        0x3A,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0xC000);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0x99);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0xC001);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(memory.ReadByte(0xC001) == 0x42);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0xC000);
}

MINTBOY_TEST(cpu_executes_relative_jumps)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x18,
        0x02,
        0x3E,
        0x00,
        0x3E,
        0x42,
        0xFE,
        0x42,
        0x20,
        0x02,
        0x3E,
        0x24,
        0x28,
        0x02,
        0x3E,
        0x00,
        0x3E,
        0x11,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0104);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x010A);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x24);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0110);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x11);
}

MINTBOY_TEST(cpu_calls_and_returns)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x31,
        0x00,
        0xC1,
        0xCD,
        0x08,
        0x01,
        0x3E,
        0x42,
        0x3E,
        0x24,
        0xC9,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 24);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0108);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xC0FE);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x24);
    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0106);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xC100);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);
}

MINTBOY_TEST(cpu_pushes_and_pops_register_pairs)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x31,
        0x00,
        0xC1,
        0x01,
        0x34,
        0x12,
        0xC5,
        0x01,
        0x00,
        0x00,
        0xC1,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().BC() == 0x1234);
    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xC0FE);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().BC() == 0x0000);
    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().BC() == 0x1234);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xC100);
}

MINTBOY_TEST(cpu_executes_register_to_register_loads)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x21,
        0x00,
        0xC0,
        0x36,
        0x55,
        0x46,
        0x78,
        0x77,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0x55);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().b == 0x55);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x55);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0x55);
}

MINTBOY_TEST(cpu_executes_alu_register_operations)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x0F,
        0x06,
        0x01,
        0x80,
        0x90,
        0xA0,
        0xB0,
        0xAF,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 8);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x10);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::HalfCarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x0F);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::SubtractFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x01);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::HalfCarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x01);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x00);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) != 0);
}

MINTBOY_TEST(cpu_executes_immediate_alu_operations)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x80,
        0xC6,
        0x80,
        0xF6,
        0x0F,
        0xE6,
        0x07,
        0xEE,
        0x07,
        0xFE,
        0x00,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x00);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) != 0);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x0F);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x07);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x00);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) != 0);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::SubtractFlag) != 0);
}

MINTBOY_TEST(cpu_executes_conditional_absolute_control_flow)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x01,
        0xFE,
        0x01,
        0xCA,
        0x0A,
        0x01,
        0x3E,
        0x00,
        0x00,
        0x31,
        0x00,
        0xC1,
        0xCC,
        0x14,
        0x01,
        0x3E,
        0x42,
        0x00,
        0x00,
        0xC8,
        0x3E,
        0x24,
        0xC9,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x010A);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 24);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0114);

    MINTBOY_REQUIRE(cpu.Step() == 20);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0110);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);
}

MINTBOY_TEST(cpu_executes_more_16bit_operations)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x31,
        0xF8,
        0xFF,
        0x21,
        0x08,
        0x00,
        0x39,
        0x11,
        0x01,
        0x00,
        0x19,
        0x33,
        0xF8,
        0xFE,
        0xF9,
        0x08,
        0x00,
        0xC0,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xFFF8);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0x0008);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0x0000);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0x0001);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xFFF9);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.GetRegisters().HL() == 0xFFF7);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xFFF7);

    MINTBOY_REQUIRE(cpu.Step() == 20);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0xF7);
    MINTBOY_REQUIRE(memory.ReadByte(0xC001) == 0xFF);
}

MINTBOY_TEST(cpu_executes_carry_condition_control_flow)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x00,
        0xD6,
        0x01,
        0xDA,
        0x09,
        0x01,
        0x3E,
        0x00,
        0x31,
        0x00,
        0xC1,
        0xDC,
        0x13,
        0x01,
        0x3E,
        0x42,
        0x00,
        0x00,
        0xD8,
        0x3E,
        0x24,
        0xC9,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0109);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 24);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0113);

    MINTBOY_REQUIRE(cpu.Step() == 20);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x010F);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);
}

MINTBOY_TEST(cpu_executes_cb_rotate_shift_and_swap_registers)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x06,
        0x81,
        0xCB,
        0x00,
        0xCB,
        0x38,
        0x0E,
        0x10,
        0x37,
        0xCB,
        0x11,
        0x16,
        0xF0,
        0xCB,
        0x32,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().b == 0x03);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().b == 0x01);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 4);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().c == 0x21);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) == 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().d == 0x0F);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) == 0);
}

MINTBOY_TEST(cpu_executes_cb_bit_res_set_registers)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x80,
        0xCB,
        0x7F,
        0xCB,
        0xBF,
        0xCB,
        0x7F,
        0xCB,
        0xC7,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) == 0);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::HalfCarryFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x00);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x01);
}

MINTBOY_TEST(cpu_executes_cb_operations_on_hl_memory)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x21,
        0x00,
        0xC0,
        0x36,
        0x01,
        0xCB,
        0x06,
        0xCB,
        0x46,
        0xCB,
        0x86,
        0xCB,
        0xC6,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0x01);

    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0x02);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) == 0);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::ZeroFlag) != 0);

    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0x02);

    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(memory.ReadByte(0xC000) == 0x03);
}

MINTBOY_TEST(cpu_executes_daa_after_add_and_subtract)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x3E,
        0x15,
        0xC6,
        0x27,
        0x27,
        0xD6,
        0x12,
        0x27,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x3C);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x42);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::CarryFlag) == 0);

    MINTBOY_REQUIRE(cpu.Step() == 8);
    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a == 0x30);
    MINTBOY_REQUIRE((cpu.GetRegisters().f & mintboy::Registers::SubtractFlag) != 0);
}

MINTBOY_TEST(cpu_executes_rst_and_reti)
{
    std::vector<mintboy::Byte> rom(0x8000, 0);
    rom[0x0038] = 0xD9;
    rom[0x0100] = 0x31;
    rom[0x0101] = 0x00;
    rom[0x0102] = 0xC1;
    rom[0x0103] = 0xFF;

    mintboy::Cartridge cartridge(std::move(rom));
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 12);
    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0038);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xC0FE);

    MINTBOY_REQUIRE(cpu.Step() == 16);
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0104);
    MINTBOY_REQUIRE(cpu.GetRegisters().sp == 0xC100);
}

MINTBOY_TEST(cpu_executes_stop)
{
    mintboy::Cartridge cartridge = MakeRom({
        0x10,
        0x00,
        0x3E,
        0x42,
    });
    mintboy::Memory memory(cartridge);
    mintboy::Cpu cpu(memory);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.IsStopped());
    MINTBOY_REQUIRE(cpu.GetRegisters().pc == 0x0102);

    MINTBOY_REQUIRE(cpu.Step() == 4);
    MINTBOY_REQUIRE(cpu.GetRegisters().a != 0x42);
}
