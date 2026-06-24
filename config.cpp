#include "stdafx.h"
#include "config.h"

namespace podcast_cfg {

	// clang-format off
	static constexpr GUID guid_color_mode    = { 0x3d7e9a14, 0x6b2c, 0x4f81, { 0x9a, 0x52, 0x8e, 0x3f, 0x71, 0x0c, 0x42, 0x01 } };
	static constexpr GUID guid_custom_bg     = { 0x3d7e9a14, 0x6b2c, 0x4f81, { 0x9a, 0x52, 0x8e, 0x3f, 0x71, 0x0c, 0x42, 0x02 } };
	static constexpr GUID guid_custom_text   = { 0x3d7e9a14, 0x6b2c, 0x4f81, { 0x9a, 0x52, 0x8e, 0x3f, 0x71, 0x0c, 0x42, 0x03 } };
	// clang-format on

	cfg_uint color_mode(guid_color_mode, color_mode_theme);
	cfg_uint custom_bg_color(guid_custom_bg, (t_uint32)RGB(255, 255, 255));
	cfg_uint custom_text_color(guid_custom_text, (t_uint32)RGB(0, 0, 0));

}
