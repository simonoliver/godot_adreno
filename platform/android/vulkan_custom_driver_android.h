/**************************************************************************/
/*  vulkan_custom_driver_android.h                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#ifdef VULKAN_ENABLED

#include <drivers/vulkan/godot_vulkan.h>

// Loads a custom Vulkan driver (e.g. Mesa Turnip for Qualcomm Adreno) through adrenotools, as configured by the
// "rendering/rendering_device/vulkan/android_custom_driver" project setting. See its documentation.
class VulkanCustomDriverAndroid {
public:
	// The custom driver's vkGetInstanceProcAddr, or nullptr to use the system Vulkan loader.
	static PFN_vkGetInstanceProcAddr load();
	// Called once the rendering device and screen are up: a crash before this makes the next launch use the system
	// driver.
	static void mark_started();
	// Whether load() returned a custom driver.
	static bool is_loaded();
};

#endif // VULKAN_ENABLED
