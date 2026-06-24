#pragma once

namespace podcast_cfg {

	enum {
		color_mode_theme = 0,   // follow foobar2000's UI colors / dark mode
		color_mode_custom = 1,  // user-chosen background/text colors
	};

	extern cfg_uint color_mode;
	extern cfg_uint custom_bg_color;
	extern cfg_uint custom_text_color;

}
