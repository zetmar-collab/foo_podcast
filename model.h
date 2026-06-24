#pragma once

namespace podcast {

	struct Episode {
		pfc::string8 guid;
		pfc::string8 title;
		pfc::string8 url;
		time_t pubDate = 0;
		long durationSeconds = 0;
		bool listened = false;
	};

	struct Channel {
		pfc::string8 feedUrl;
		pfc::string8 title;
		pfc::list_t<Episode> episodes;
	};

	class Library {
	public:
		pfc::list_t<Channel> channels;

		t_size find_channel(const char* feedUrl) const;
		t_size find_episode(const Channel& ch, const char* guid) const;

		// Merges freshly fetched channel/episode data into the library,
		// preserving the listened flag of episodes that already existed.
		void merge_channel(const Channel& fresh);

		void remove_channel(t_size index);

		void load();
		void save() const;

	private:
		pfc::string8 storage_path() const;
	};

	// Process-wide library instance, lazily loaded from disk on first access.
	Library& GetLibrary();

}
