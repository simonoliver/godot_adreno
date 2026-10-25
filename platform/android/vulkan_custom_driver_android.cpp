/**************************************************************************/
/*  vulkan_custom_driver_android.cpp                                      */
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

#include "vulkan_custom_driver_android.h"

#ifdef VULKAN_ENABLED

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"

#ifdef ADRENOTOOLS_ENABLED
#include <adrenotools/driver.h>
#endif

#include <dlfcn.h>
#include <unistd.h>

static const char *SETTING = "rendering/rendering_device/vulkan/android_custom_driver";
// Exists while a launch is loading the custom driver; left behind by a crash.
static const char *STARTING_MARKER = ".vulkan_custom_driver_starting";
// Created by the game (e.g. an options menu) to opt out.
static const char *USE_SYSTEM_MARKER = "use_system_vulkan_driver";

static bool custom_driver_loaded = false;

#ifdef ADRENOTOOLS_ENABLED
// Directory of libgodot_android.so: the app's nativeLibraryDir, where the adrenotools hook libraries are packaged.
static String _native_library_dir() {
	Dl_info info;
	if (dladdr((void *)&_native_library_dir, &info) == 0 || info.dli_fname == nullptr) {
		return String();
	}
	return String::utf8(info.dli_fname).get_base_dir();
}

// Copies every file of p_from (res://) into p_to, skipping files that already have the same size.
static bool _copy_driver_files(const String &p_from, const String &p_to) {
	Ref<DirAccess> source = DirAccess::open(p_from);
	ERR_FAIL_COND_V_MSG(source.is_null(), false, vformat("Vulkan custom driver: can't open \"%s\".", p_from));
	if (DirAccess::make_dir_recursive_absolute(p_to) != OK) {
		ERR_FAIL_V_MSG(false, vformat("Vulkan custom driver: can't create \"%s\".", p_to));
	}
	for (const String &file : source->get_files()) {
		const String from = p_from.path_join(file);
		const String to = p_to.path_join(file);
		Ref<FileAccess> existing = FileAccess::open(to, FileAccess::READ);
		Ref<FileAccess> in = FileAccess::open(from, FileAccess::READ);
		ERR_FAIL_COND_V(in.is_null(), false);
		if (existing.is_valid() && existing->get_length() == in->get_length()) {
			continue;
		}
		existing.unref();
		Ref<FileAccess> out = FileAccess::open(to, FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(out.is_null(), false, vformat("Vulkan custom driver: can't write \"%s\".", to));
		out->store_buffer(in->get_buffer(in->get_length()));
	}
	return true;
}
#endif

PFN_vkGetInstanceProcAddr VulkanCustomDriverAndroid::load() {
#ifdef ADRENOTOOLS_ENABLED
	if (!ProjectSettings::get_singleton()->has_setting(SETTING)) {
		return nullptr;
	}
	const String source = GLOBAL_GET(SETTING);
	if (source.is_empty()) {
		return nullptr;
	}
	const String data_dir = OS::get_singleton()->get_user_data_dir();
	if (FileAccess::exists(data_dir.path_join(USE_SYSTEM_MARKER))) {
		print_line("Vulkan custom driver: disabled by user://" + String(USE_SYSTEM_MARKER) + ", using the system driver.");
		return nullptr;
	}
	// adrenotools replaces the Qualcomm (kgsl) driver; other GPUs keep their own.
	if (access("/dev/kgsl-3d0", F_OK) != 0) {
		return nullptr;
	}
	const String starting = data_dir.path_join(STARTING_MARKER);
	if (FileAccess::exists(starting)) {
		DirAccess::remove_absolute(starting);
		WARN_PRINT("Vulkan custom driver: the previous launch didn't finish starting with it; using the system driver this time.");
		return nullptr;
	}

	const Variant meta = JSON::parse_string(FileAccess::get_file_as_string(source.path_join("meta.json")));
	ERR_FAIL_COND_V_MSG(meta.get_type() != Variant::DICTIONARY, nullptr, vformat("Vulkan custom driver: no valid meta.json in \"%s\".", source));
	const Dictionary meta_dict = meta;
	const String library_name = meta_dict.get("libraryName", "");
	ERR_FAIL_COND_V_MSG(library_name.is_empty(), nullptr, "Vulkan custom driver: meta.json has no libraryName.");

	const String driver_dir = data_dir.path_join("vulkan_driver").path_join(source.trim_suffix("/").get_file());
	if (!_copy_driver_files(source, driver_dir)) {
		return nullptr;
	}
	const String tmp_dir = data_dir.path_join("vulkan_driver_tmp");
	DirAccess::make_dir_recursive_absolute(tmp_dir);
	const String hook_dir = _native_library_dir();
	ERR_FAIL_COND_V_MSG(hook_dir.is_empty(), nullptr, "Vulkan custom driver: can't find the native library directory.");

	{
		Ref<FileAccess> marker = FileAccess::open(starting, FileAccess::WRITE);
	}
	// adrenotools expects directories with a trailing slash.
	void *handle = adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM, (tmp_dir + "/").utf8().get_data(),
			(hook_dir + "/").utf8().get_data(), (driver_dir + "/").utf8().get_data(), library_name.utf8().get_data(), nullptr, nullptr);
	if (handle == nullptr) {
		DirAccess::remove_absolute(starting);
		ERR_FAIL_V_MSG(nullptr, vformat("Vulkan custom driver: adrenotools couldn't load \"%s\"; using the system driver.", library_name));
	}
	PFN_vkGetInstanceProcAddr proc_addr = (PFN_vkGetInstanceProcAddr)dlsym(handle, "vkGetInstanceProcAddr");
	if (proc_addr == nullptr) {
		DirAccess::remove_absolute(starting);
		ERR_FAIL_V_MSG(nullptr, "Vulkan custom driver: no vkGetInstanceProcAddr; using the system driver.");
	}
	print_line(vformat("Vulkan custom driver: %s (%s, %s).", meta_dict.get("name", library_name), meta_dict.get("driverVersion", "?"), library_name));
	custom_driver_loaded = true;
	return proc_addr;
#else
	return nullptr;
#endif
}

void VulkanCustomDriverAndroid::mark_started() {
	if (custom_driver_loaded) {
		DirAccess::remove_absolute(OS::get_singleton()->get_user_data_dir().path_join(STARTING_MARKER));
	}
}

bool VulkanCustomDriverAndroid::is_loaded() {
	return custom_driver_loaded;
}

#endif // VULKAN_ENABLED
