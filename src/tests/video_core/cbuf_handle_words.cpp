// SPDX-License-Identifier: GPL-3.0-or-later

#include <map>
#include <sstream>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "video_core/pipeline_cache_files.h"
#include "video_core/shader_environment.h"

namespace {

/// A compute program's environment answering from fixed constant buffer words and textures, and
/// recording its reads as the backends' environments do.
class WordEnvironment final : public VideoCommon::GenericEnvironment {
public:
    WordEnvironment(std::map<std::pair<u32, u32>, u32> words_,
                    std::map<u32, Shader::TextureType> textures_)
        : words{std::move(words_)}, textures{std::move(textures_)} {
        stage = Shader::Stage::Compute;
        code = {0x123456789abcdef0};
        cached_lowest = 0;
        cached_highest = 0;
        texture_bound = 2;
    }

    u32 ReadCbufValue(u32 index, u32 offset) override {
        const auto it = words.find({index, offset});
        const u32 value = it == words.end() ? 0 : it->second;
        RecordCbufValue(index, offset, value);
        return value;
    }
    Shader::TextureType ReadTextureType(u32 handle) override {
        const auto it = textures.find(handle);
        const auto type = it == textures.end() ? Shader::TextureType::Color2D : it->second;
        texture_types.emplace(handle, type);
        return type;
    }
    Shader::TexturePixelFormat ReadTexturePixelFormat(u32) override {
        return {};
    }
    bool IsTexturePixelFormatInteger(u32) override {
        return false;
    }
    u32 ReadViewportTransformState() override {
        return 1;
    }
    std::optional<Shader::ReplaceConstant> GetReplaceConstBuffer(u32, u32) override {
        return std::nullopt;
    }

private:
    std::map<std::pair<u32, u32>, u32> words;
    std::map<u32, Shader::TextureType> textures;
};

VideoCommon::FileEnvironment Record(WordEnvironment& env) {
    std::stringstream stream;
    env.Serialize(stream, 0);
    const auto end = stream.tellp();
    stream.seekg(0);
    VideoCommon::FileEnvironment file;
    file.Deserialize(stream, end);
    return file;
}

constexpr auto Color2D = Shader::TextureType::Color2D;
constexpr auto Color3D = Shader::TextureType::Color3D;

} // Anonymous namespace

TEST_CASE("Handle word marker keys", "[video_core]") {
    const auto key = VideoCommon::CbufHandleWordKey(17, 0xfffc);
    REQUIRE(key.has_value());
    REQUIRE(VideoCommon::CbufHandleWordFromKey(*key) == ((u64{17} << 32) | 0xfffc));
    REQUIRE(!VideoCommon::CbufHandleWordKey(0, 0x10000).has_value());
    // The read properties and the null texture marker are no handle words.
    REQUIRE(!VideoCommon::CbufHandleWordFromKey(u64{~0U} << 32).has_value());
    REQUIRE(!VideoCommon::CbufHandleWordFromKey((u64{~0U} << 32) | 1).has_value());
    REQUIRE(!VideoCommon::CbufHandleWordFromKey((u64{2} << 32) | 0x80000000).has_value());
}

TEST_CASE("Only words read as handles alone may change value", "[video_core]") {
    // Word (2, 0x10) is a handle; word (1, 0x20) is an ordinary value that equals handle 7.
    WordEnvironment recorded{{{{2, 0x10}, 7}, {{1, 0x20}, 7}}, {{7, Color2D}}};
    (void)recorded.ReadTextureType(recorded.ReadCbufHandleWord(2, 0x10));
    (void)recorded.ReadCbufValue(1, 0x20);
    auto file = Record(recorded);

    SECTION("Another handle naming a texture of the same type") {
        WordEnvironment live{{{{2, 0x10}, 9}, {{1, 0x20}, 7}}, {{7, Color2D}, {9, Color2D}}};
        const char* why = "";
        REQUIRE(file.AnswersHoldFor(live, &why));
        // The live environment notes the word as a handle word too, for the disk cache.
        auto rerecorded = Record(live);
        WordEnvironment next{{{{2, 0x10}, 11}, {{1, 0x20}, 7}}, {{7, Color2D}, {11, Color2D}}};
        REQUIRE(rerecorded.AnswersHoldFor(next, &why));
    }
    SECTION("Another handle naming a texture of another type") {
        WordEnvironment live{{{{2, 0x10}, 9}, {{1, 0x20}, 7}}, {{7, Color2D}, {9, Color3D}}};
        const char* why = "";
        REQUIRE(!file.AnswersHoldFor(live, &why));
        REQUIRE(std::string_view{why} == "texture type");
    }
    SECTION("An ordinary value that changed to another handle's value") {
        WordEnvironment live{{{{2, 0x10}, 7}, {{1, 0x20}, 9}}, {{7, Color2D}, {9, Color2D}}};
        const char* why = "";
        REQUIRE(!file.AnswersHoldFor(live, &why));
        REQUIRE(std::string_view{why} == "constant buffer value");
    }
}

TEST_CASE("A word read as a handle and as a value keeps its value", "[video_core]") {
    WordEnvironment recorded{{{{2, 0x10}, 7}}, {{7, Color2D}}};
    (void)recorded.ReadTextureType(recorded.ReadCbufHandleWord(2, 0x10));
    (void)recorded.ReadCbufValue(2, 0x10);
    auto file = Record(recorded);
    WordEnvironment live{{{{2, 0x10}, 9}}, {{7, Color2D}, {9, Color2D}}};
    const char* why = "";
    REQUIRE(!file.AnswersHoldFor(live, &why));
    REQUIRE(std::string_view{why} == "constant buffer value");
}
