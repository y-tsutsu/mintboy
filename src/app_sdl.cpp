#include "mintboy/cartridge.hpp"
#include "mintboy/cpu.hpp"
#include "mintboy/memory.hpp"

#include <SDL.h>
#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr int DefaultWindowScale = 4;
    constexpr int CyclesPerFrame = 70224;
    constexpr int AudioSampleRate = 48000;
    constexpr float DefaultAudioVolume = 0.5F;
    constexpr float MaximumAudioGain = 0.6F;
    constexpr const char *ConfigFileName = "mintboy.toml";

    bool g_trace_input_enabled = false;

    struct AppSettings
    {
        std::optional<std::filesystem::path> rom_path;
        bool swap_controller_ab = false;
        bool trace_input = false;
        int window_scale = DefaultWindowScale;
        bool audio_enabled = true;
        float audio_volume = DefaultAudioVolume;
    };

    struct DebugControls
    {
        bool paused = false;
        bool step_frame = false;
        int speed_index = 1;
    };

    bool TraceInputEnabled()
    {
        return g_trace_input_enabled;
    }

    bool EnvironmentFlag(const char *name, bool fallback)
    {
        const char *value = std::getenv(name);
        if (value == nullptr)
        {
            return fallback;
        }

        const std::string text(value);
        return text != "0" && text != "false" && text != "False" && text != "FALSE";
    }

    void ApplyTomlConfig(AppSettings &settings, const std::filesystem::path &path)
    {
        const toml::table config = toml::parse_file(path.string());

        if (const auto value = config["rom"]["path"].value<std::string>())
        {
            std::filesystem::path rom_path = *value;
            if (rom_path.is_relative())
            {
                rom_path = path.parent_path() / rom_path;
            }
            settings.rom_path = rom_path;
        }
        if (const auto value = config["input"]["swap_controller_ab"].value<bool>())
        {
            settings.swap_controller_ab = *value;
        }
        if (const auto value = config["input"]["trace_input"].value<bool>())
        {
            settings.trace_input = *value;
        }
        if (const auto value = config["video"]["window_scale"].value<int64_t>())
        {
            if (*value <= 0 || *value > 16)
            {
                throw std::runtime_error("video.window_scale must be between 1 and 16");
            }
            settings.window_scale = static_cast<int>(*value);
        }
        if (const auto value = config["audio"]["enabled"].value<bool>())
        {
            settings.audio_enabled = *value;
        }
        if (const auto value = config["audio"]["volume"].value<double>())
        {
            if (*value < 0.0 || *value > 1.0)
            {
                throw std::runtime_error("audio.volume must be between 0.0 and 1.0");
            }
            settings.audio_volume = static_cast<float>(*value);
        }
    }

    AppSettings LoadSettings(const char *executable_path)
    {
        AppSettings settings;

        const std::array<std::filesystem::path, 2> config_paths = {
            std::filesystem::path(ConfigFileName),
            std::filesystem::path(executable_path).parent_path() / ConfigFileName,
        };

        std::optional<std::filesystem::path> loaded_path;
        for (const auto &path : config_paths)
        {
            if (path.empty() || loaded_path == path || !std::filesystem::exists(path))
            {
                continue;
            }
            ApplyTomlConfig(settings, path);
            loaded_path = path;
            break;
        }

        settings.swap_controller_ab = EnvironmentFlag("MINTBOY_SWAP_CONTROLLER_AB", settings.swap_controller_ab);
        settings.trace_input = EnvironmentFlag("MINTBOY_TRACE_INPUT", settings.trace_input);
        return settings;
    }

    double SpeedMultiplier(int speed_index)
    {
        switch (speed_index)
        {
        case 0:
            return 0.5;
        case 2:
            return 2.0;
        default:
            return 1.0;
        }
    }

    std::string SpeedLabel(int speed_index)
    {
        switch (speed_index)
        {
        case 0:
            return "0.5x";
        case 2:
            return "2x";
        default:
            return "1x";
        }
    }

    class Sdl
    {
    public:
        Sdl()
        {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0)
            {
                throw std::runtime_error(SDL_GetError());
            }
            SDL_GameControllerEventState(SDL_ENABLE);
        }

        ~Sdl()
        {
            SDL_Quit();
        }

        Sdl(const Sdl &) = delete;
        Sdl &operator=(const Sdl &) = delete;
    };

    class Window
    {
    public:
        Window(const std::string &title, int scale)
        {
            window_ = SDL_CreateWindow(
                title.c_str(),
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                mintboy::Memory::ScreenWidth * scale,
                mintboy::Memory::ScreenHeight * scale,
                SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
            if (window_ == nullptr)
            {
                throw std::runtime_error(SDL_GetError());
            }

            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (renderer_ == nullptr)
            {
                throw std::runtime_error(SDL_GetError());
            }

            texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, mintboy::Memory::ScreenWidth, mintboy::Memory::ScreenHeight);
            if (texture_ == nullptr)
            {
                throw std::runtime_error(SDL_GetError());
            }
        }

        ~Window()
        {
            SDL_DestroyTexture(texture_);
            SDL_DestroyRenderer(renderer_);
            SDL_DestroyWindow(window_);
        }

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        void Present(const mintboy::Memory::Framebuffer &pixels)
        {
            SDL_UpdateTexture(texture_, nullptr, pixels.data(), mintboy::Memory::ScreenWidth * static_cast<int>(sizeof(std::uint32_t)));
            SDL_RenderClear(renderer_);
            SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(renderer_);
        }

        void SetTitle(const std::string &title)
        {
            SDL_SetWindowTitle(window_, title.c_str());
        }

    private:
        SDL_Window *window_ = nullptr;
        SDL_Renderer *renderer_ = nullptr;
        SDL_Texture *texture_ = nullptr;
    };

    class Audio
    {
    public:
        explicit Audio(float volume)
            : volume_(volume)
        {
            SDL_AudioSpec desired{};
            desired.freq = AudioSampleRate;
            desired.format = AUDIO_F32SYS;
            desired.channels = 1;
            desired.samples = 1024;

            device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained_, 0);
            if (device_ == 0)
            {
                throw std::runtime_error(SDL_GetError());
            }

            SDL_PauseAudioDevice(device_, 0);
        }

        ~Audio()
        {
            if (device_ != 0)
            {
                SDL_CloseAudioDevice(device_);
            }
        }

        Audio(const Audio &) = delete;
        Audio &operator=(const Audio &) = delete;

        void QueueSamples(const std::vector<float> &samples)
        {
            constexpr int MaximumQueuedSamples = AudioSampleRate * 3 / 20;
            if (QueuedSamples() < MaximumQueuedSamples && !samples.empty())
            {
                const int available_samples = MaximumQueuedSamples - QueuedSamples();
                const auto samples_to_queue = static_cast<std::size_t>(std::min<int>(available_samples, static_cast<int>(samples.size())));
                scaled_samples_.resize(samples_to_queue);
                const float gain = volume_ * MaximumAudioGain;
                for (std::size_t index = 0; index < samples_to_queue; ++index)
                {
                    scaled_samples_[index] = samples[index] * gain;
                }
                if (SDL_QueueAudio(device_, scaled_samples_.data(), static_cast<Uint32>(scaled_samples_.size() * sizeof(float))) != 0)
                {
                    throw std::runtime_error(SDL_GetError());
                }
            }

            QueueSilenceIfNeeded();
        }

    private:
        [[nodiscard]] int QueuedSamples() const
        {
            return static_cast<int>(SDL_GetQueuedAudioSize(device_) / sizeof(float));
        }

        void QueueSilenceIfNeeded()
        {
            constexpr int MinimumQueuedSamples = AudioSampleRate * 3 / 100;
            const int queued_samples = QueuedSamples();
            if (queued_samples >= MinimumQueuedSamples)
            {
                return;
            }

            const int samples_to_queue = MinimumQueuedSamples - queued_samples;
            silence_.assign(static_cast<std::size_t>(samples_to_queue), 0.0F);
            if (SDL_QueueAudio(device_, silence_.data(), static_cast<Uint32>(silence_.size() * sizeof(float))) != 0)
            {
                throw std::runtime_error(SDL_GetError());
            }
        }

        SDL_AudioDeviceID device_ = 0;
        SDL_AudioSpec obtained_{};
        std::vector<float> silence_;
        std::vector<float> scaled_samples_;
        float volume_ = DefaultAudioVolume;
    };

    class Controller
    {
    public:
        explicit Controller(bool swap_ab)
            : swap_ab_(swap_ab)
        {
            OpenFirstAvailable();
        }

        ~Controller()
        {
            Close();
        }

        Controller(const Controller &) = delete;
        Controller &operator=(const Controller &) = delete;

        void HandleEvent(const SDL_Event &event)
        {
            if (event.type == SDL_CONTROLLERDEVICEADDED && controller_ == nullptr)
            {
                Open(event.cdevice.which);
                return;
            }

            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller_ != nullptr && event.cdevice.which == instance_id_)
            {
                Close();
                OpenFirstAvailable();
            }
        }

        [[nodiscard]] bool Right() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || Axis(SDL_CONTROLLER_AXIS_LEFTX) > AxisThreshold;
        }

        [[nodiscard]] bool Left() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || Axis(SDL_CONTROLLER_AXIS_LEFTX) < -AxisThreshold;
        }

        [[nodiscard]] bool Up() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_UP) || Axis(SDL_CONTROLLER_AXIS_LEFTY) < -AxisThreshold;
        }

        [[nodiscard]] bool Down() const
        {
            return Button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || Axis(SDL_CONTROLLER_AXIS_LEFTY) > AxisThreshold;
        }

        [[nodiscard]] bool A() const
        {
            return swap_ab_
                       ? Button(SDL_CONTROLLER_BUTTON_B) || Button(SDL_CONTROLLER_BUTTON_Y)
                       : Button(SDL_CONTROLLER_BUTTON_A) || Button(SDL_CONTROLLER_BUTTON_X);
        }

        [[nodiscard]] bool B() const
        {
            return swap_ab_
                       ? Button(SDL_CONTROLLER_BUTTON_A) || Button(SDL_CONTROLLER_BUTTON_X)
                       : Button(SDL_CONTROLLER_BUTTON_B) || Button(SDL_CONTROLLER_BUTTON_Y);
        }

        [[nodiscard]] bool Select() const
        {
            return Button(SDL_CONTROLLER_BUTTON_BACK);
        }

        [[nodiscard]] bool Start() const
        {
            return Button(SDL_CONTROLLER_BUTTON_START);
        }

    private:
        static constexpr Sint16 AxisThreshold = 16000;

        void OpenFirstAvailable()
        {
            const int joystick_count = SDL_NumJoysticks();
            for (int index = 0; index < joystick_count; ++index)
            {
                if (SDL_IsGameController(index) == SDL_TRUE && Open(index))
                {
                    return;
                }
            }

            if (TraceInputEnabled())
            {
                std::cerr << "controller unavailable joysticks=" << joystick_count << '\n';
            }
        }

        bool Open(int device_index)
        {
            SDL_GameController *controller = SDL_GameControllerOpen(device_index);
            if (controller == nullptr)
            {
                if (TraceInputEnabled())
                {
                    std::cerr << "controller open failed index=" << device_index << " error=" << SDL_GetError() << '\n';
                }
                return false;
            }

            Close();
            controller_ = controller;
            SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller_);
            instance_id_ = joystick == nullptr ? -1 : SDL_JoystickInstanceID(joystick);

            if (TraceInputEnabled())
            {
                const char *name = SDL_GameControllerName(controller_);
                std::cerr << "controller opened index=" << device_index << " name=" << (name == nullptr ? "" : name) << '\n';
            }
            return true;
        }

        void Close()
        {
            if (controller_ == nullptr)
            {
                return;
            }

            if (TraceInputEnabled())
            {
                std::cerr << "controller closed instance=" << instance_id_ << '\n';
            }
            SDL_GameControllerClose(controller_);
            controller_ = nullptr;
            instance_id_ = -1;
        }

        [[nodiscard]] bool Button(SDL_GameControllerButton button) const
        {
            return controller_ != nullptr && SDL_GameControllerGetButton(controller_, button) != 0;
        }

        [[nodiscard]] Sint16 Axis(SDL_GameControllerAxis axis) const
        {
            return controller_ == nullptr ? 0 : SDL_GameControllerGetAxis(controller_, axis);
        }

        SDL_GameController *controller_ = nullptr;
        SDL_JoystickID instance_id_ = -1;
        bool swap_ab_ = true;
    };

    void SyncJoypad(mintboy::Memory &memory, const Controller &controller)
    {
        static std::uint16_t last_trace_state = 0xFFFF;
        SDL_PumpEvents();
        const std::uint8_t *keys = SDL_GetKeyboardState(nullptr);

        const bool right = keys[SDL_SCANCODE_RIGHT] != 0 || controller.Right();
        const bool left = keys[SDL_SCANCODE_LEFT] != 0 || controller.Left();
        const bool up = keys[SDL_SCANCODE_UP] != 0 || controller.Up();
        const bool down = keys[SDL_SCANCODE_DOWN] != 0 || controller.Down();
        const bool a = keys[SDL_SCANCODE_Z] != 0 || keys[SDL_SCANCODE_A] != 0 || controller.A();
        const bool b = keys[SDL_SCANCODE_X] != 0 || keys[SDL_SCANCODE_S] != 0 || controller.B();
        const bool select = keys[SDL_SCANCODE_BACKSPACE] != 0 || controller.Select();
        const bool start = keys[SDL_SCANCODE_RETURN] != 0 || controller.Start();

        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Right, right);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Left, left);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Up, up);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Down, down);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::A, a);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::B, b);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Select, select);
        memory.SetJoypadButton(mintboy::Memory::JoypadButton::Start, start);

        if (TraceInputEnabled())
        {
            std::uint16_t trace_state = 0;
            trace_state |= right ? 1 << 0 : 0;
            trace_state |= left ? 1 << 1 : 0;
            trace_state |= up ? 1 << 2 : 0;
            trace_state |= down ? 1 << 3 : 0;
            trace_state |= a ? 1 << 4 : 0;
            trace_state |= b ? 1 << 5 : 0;
            trace_state |= select ? 1 << 6 : 0;
            trace_state |= start ? 1 << 7 : 0;
            if (trace_state == last_trace_state)
            {
                return;
            }
            last_trace_state = trace_state;

            std::cerr << "input state"
                      << " right=" << right
                      << " left=" << left
                      << " up=" << up
                      << " down=" << down
                      << " a=" << a
                      << " b=" << b
                      << " select=" << select
                      << " start=" << start
                      << '\n';
        }
    }

    bool PollEvents(Controller &controller, DebugControls &debug_controls)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0)
        {
            controller.HandleEvent(event);

            if (TraceInputEnabled() && (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP))
            {
                std::cerr << "input event"
                          << " type=" << (event.type == SDL_KEYDOWN ? "down" : "up")
                          << " scancode=" << SDL_GetScancodeName(event.key.keysym.scancode)
                          << " key=" << SDL_GetKeyName(event.key.keysym.sym)
                          << " repeat=" << static_cast<int>(event.key.repeat)
                          << '\n';
            }

            if (event.type == SDL_QUIT)
            {
                return true;
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                return true;
            }

            if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_p:
                    debug_controls.paused = !debug_controls.paused;
                    break;
                case SDLK_PERIOD:
                    debug_controls.paused = true;
                    debug_controls.step_frame = true;
                    break;
                case SDLK_1:
                    debug_controls.speed_index = 1;
                    break;
                case SDLK_2:
                    debug_controls.speed_index = 2;
                    break;
                case SDLK_3:
                    debug_controls.speed_index = 0;
                    break;
                default:
                    break;
                }
            }
        }

        return false;
    }

    std::string WindowTitle(const std::string &rom_title, const DebugControls &debug_controls)
    {
        std::string title = "mintboy - " + rom_title + " [" + SpeedLabel(debug_controls.speed_index) + "]";
        if (debug_controls.paused)
        {
            title += " paused";
        }
        return title;
    }
}

int main(int argc, char **argv)
{
    SDL_SetMainReady();

    if (argc > 2)
    {
        std::cerr << "usage: mintboy [rom.gb]\n";
        return 2;
    }

    try
    {
        AppSettings settings = LoadSettings(argv[0]);
        if (argc == 2)
        {
            settings.rom_path = argv[1];
        }
        if (!settings.rom_path.has_value())
        {
            throw std::runtime_error("no ROM path specified; pass a ROM path or set rom.path in mintboy.toml");
        }
        g_trace_input_enabled = settings.trace_input;

        mintboy::Cartridge cartridge = mintboy::Cartridge::LoadFromFile(settings.rom_path->string());
        if (cartridge.RequiresCgb())
        {
            std::cerr << "warning: this ROM requires Game Boy Color hardware; mintboy currently runs in DMG mode\n";
        }
        mintboy::Memory memory(cartridge);
        mintboy::Cpu cpu(memory);

        const Sdl sdl;
        const std::string rom_title = cartridge.Title().empty()
                                          ? settings.rom_path->filename().string()
                                          : cartridge.Title();
        Window window(WindowTitle(rom_title, DebugControls{}), settings.window_scale);
        std::optional<Audio> audio;
        if (settings.audio_enabled)
        {
            audio.emplace(settings.audio_volume);
        }
        Controller controller(settings.swap_controller_ab);
        DebugControls debug_controls;
        std::string current_window_title;

        bool running = true;
        bool cpu_running = true;
        int frames_since_save = 0;
        while (running)
        {
            running = !PollEvents(controller, debug_controls);
            SyncJoypad(memory, controller);

            const bool run_frame = !debug_controls.paused || debug_controls.step_frame;
            const int target_cycles = debug_controls.step_frame
                                          ? CyclesPerFrame
                                          : static_cast<int>(CyclesPerFrame * SpeedMultiplier(debug_controls.speed_index));
            int cycles = 0;
            while (run_frame && cycles < target_cycles && cpu_running)
            {
                try
                {
                    cycles += cpu.Step();
                }
                catch (const std::exception &error)
                {
                    std::cerr << "CPU stopped: " << error.what() << '\n';
                    cpu_running = false;
                    break;
                }
            }
            debug_controls.step_frame = false;

            window.Present(memory.GetFramebuffer());
            const auto audio_samples = memory.DrainAudioSamples();
            if (audio.has_value())
            {
                audio->QueueSamples(audio_samples);
            }
            const std::string next_window_title = WindowTitle(rom_title, debug_controls);
            if (next_window_title != current_window_title)
            {
                current_window_title = next_window_title;
                window.SetTitle(current_window_title);
            }
            ++frames_since_save;
            if (frames_since_save >= 60)
            {
                cartridge.SaveRam();
                frames_since_save = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        cartridge.SaveRam();
    }
    catch (const std::exception &error)
    {
        std::cerr << "mintboy: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
