// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

// The vendored FFX API headers declare their entry points with MSVC's __declspec; the runtime is
// only called through pointers.
#ifndef _WIN32
#define __declspec(x)
#endif
#include "api/include/ffx_api.h"
#ifndef _WIN32
#undef __declspec
#endif

#include "video_core/vulkan_common/ffx_files.h"
#include "video_core/vulkan_common/vulkan.h"

namespace Vulkan::FfxRuntime {

/// The FFX API entry points of the runtime: AMD's loader on Windows, the bundled fsr4-vk library
/// on Linux.
struct Api {
    PfnFfxCreateContext create_context{};
    PfnFfxDestroyContext destroy_context{};
    PfnFfxQuery query{};
    PfnFfxDispatch dispatch{};
};

/// Whether FSR 4 is turned on and this driver can run it: AMD's on Windows, RADV with a library
/// on Linux (EDEN_NX2_FSR4_LIB turns it on without the setting).
[[nodiscard]] bool IsWanted(VkDriverId driver_id);

/// Loads the runtime, or null when it can't be used (the log says why). On Windows every file is
/// checked first; files that passed are not checked again this session, and the runtime stays
/// loaded while a returned pointer lives. On Linux the library comes from EDEN_NX2_FSR4_LIB, the
/// user folder or next to the program, and stays loaded.
[[nodiscard]] std::shared_ptr<const Api> Acquire();

} // namespace Vulkan::FfxRuntime
