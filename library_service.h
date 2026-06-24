#pragma once

#include <functional>

namespace library_service {

	// All of these run the network/parsing work on a background thread with
	// a progress dialog, then merge results into podcast::GetLibrary(),
	// save it to disk, and invoke onDone() on the UI thread.

	void AddPodcastByUrl(const char* feedUrl, fb2k::hwnd_t parent, std::function<void()> onDone);
	void RefreshChannel(t_size channelIndex, fb2k::hwnd_t parent, std::function<void()> onDone);
	void RefreshAll(fb2k::hwnd_t parent, std::function<void()> onDone);
	void ImportOpmlFile(const char* path, fb2k::hwnd_t parent, std::function<void()> onDone);

	// Synchronous, no network access.
	bool ExportOpmlFile(const char* path);

}
