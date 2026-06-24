#pragma once

// Shows a simple modal "prompt + edit box" dialog. Returns true and fills
// p_value with the entered text if the user pressed OK, false if cancelled.
bool ShowPodcastInputDialog(HWND parent, const char* title, const char* prompt, const char* initialValue, pfc::string8& p_value);
