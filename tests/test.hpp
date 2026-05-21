#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mintboy::test
{
    using TestFn = std::function<void()>;

    struct TestCase
    {
        std::string name;
        TestFn run;
    };

    inline std::vector<TestCase> &Registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline void Register(std::string name, TestFn run)
    {
        Registry().push_back({std::move(name), std::move(run)});
    }

    inline void Require(bool condition, const char *expression, const char *file, int line)
    {
        if (!condition)
        {
            throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": requirement failed: " + expression);
        }
    }

    inline int RunAll()
    {
        int failures = 0;

        for (const auto &test : Registry())
        {
            try
            {
                test.run();
                std::cout << "[pass] " << test.name << '\n';
            }
            catch (const std::exception &error)
            {
                ++failures;
                std::cerr << "[fail] " << test.name << ": " << error.what() << '\n';
            }
        }

        return failures == 0 ? 0 : 1;
    }

    struct AutoRegister
    {
        AutoRegister(std::string name, TestFn run)
        {
            Register(std::move(name), std::move(run));
        }
    };
}

#define MINTBOY_TEST(name)                                                       \
    static void name();                                                          \
    static const ::mintboy::test::AutoRegister name##_registration(#name, name); \
    static void name()

#define MINTBOY_REQUIRE(expression) \
    ::mintboy::test::Require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
