#include "stdafx.h"
#include "library_service.h"
#include "model.h"
#include "rss.h"
#include "opml.h"
#include "http_fetch.h"
#include <memory>
#include <vector>

namespace library_service {

	namespace {
		pfc::string8 ReadLocalFile(const char* path) {
			pfc::stringcvt::string_wide_from_utf8 pathW(path);
			HANDLE f = CreateFileW(pathW.get_ptr(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (f == INVALID_HANDLE_VALUE) return pfc::string8();
			DWORD size = GetFileSize(f, nullptr);
			pfc::array_t<char> data;
			data.set_size(size);
			DWORD readBytes = 0;
			ReadFile(f, data.get_ptr(), size, &readBytes, nullptr);
			CloseHandle(f);
			pfc::string8 out;
			out.set_string(data.get_ptr(), readBytes);
			return out;
		}

		bool WriteLocalFile(const char* path, const pfc::string8& content) {
			pfc::stringcvt::string_wide_from_utf8 pathW(path);
			HANDLE f = CreateFileW(pathW.get_ptr(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (f == INVALID_HANDLE_VALUE) return false;
			DWORD written = 0;
			BOOL ok = WriteFile(f, content.get_ptr(), (DWORD)content.length(), &written, nullptr);
			CloseHandle(f);
			return ok != 0;
		}

		// Fetches and parses a single feed. Throws podcast_http::exception_http
		// on network failure; returns false (no throw) if the body isn't a
		// recognizable feed.
		bool FetchOneChannel(const char* feedUrl, abort_callback& abort, podcast::Channel& out) {
			pfc::string8 xmlText = podcast_http::get(feedUrl, abort);
			out.feedUrl = feedUrl;
			if (!rss::ParseFeed(xmlText, out)) return false;
			if (out.title.is_empty()) out.title = feedUrl;
			return true;
		}
	}

	void AddPodcastByUrl(const char* feedUrl, fb2k::hwnd_t parent, std::function<void()> onDone) {
		auto url = std::make_shared<pfc::string8>(feedUrl);
		auto result = std::make_shared<podcast::Channel>();
		auto ok = std::make_shared<bool>(false);
		auto errMsg = std::make_shared<pfc::string8>();

		auto cb = threaded_process_callback_lambda::create(
			[](fb2k::hwnd_t) {},
			[url, result, ok, errMsg](threaded_process_status& status, abort_callback& abort) {
				status.set_item("Fetching feed...");
				try {
					*ok = FetchOneChannel(url->get_ptr(), abort, *result);
					if (!*ok) *errMsg = "This URL doesn't look like a podcast RSS or Atom feed.";
				} catch (podcast_http::exception_http& e) {
					*errMsg = e.what();
				}
			},
			[result, ok, errMsg, onDone](fb2k::hwnd_t wnd, bool aborted) {
				if (aborted) return;
				if (!*ok) {
					popup_message::g_show(errMsg->get_ptr(), "Podcasts: could not add feed");
					return;
				}
				podcast::GetLibrary().merge_channel(*result);
				podcast::GetLibrary().save();
				if (onDone) onDone();
			}
		);
		threaded_process::g_run_modeless(cb, threaded_process::flag_show_abort | threaded_process::flag_show_item, parent, "Adding podcast");
	}

	void RefreshChannel(t_size channelIndex, fb2k::hwnd_t parent, std::function<void()> onDone) {
		if (channelIndex >= podcast::GetLibrary().channels.get_count()) return;
		pfc::string8 feedUrl = podcast::GetLibrary().channels[channelIndex].feedUrl;

		auto url = std::make_shared<pfc::string8>(feedUrl);
		auto result = std::make_shared<podcast::Channel>();
		auto ok = std::make_shared<bool>(false);
		auto errMsg = std::make_shared<pfc::string8>();

		auto cb = threaded_process_callback_lambda::create(
			[](fb2k::hwnd_t) {},
			[url, result, ok, errMsg](threaded_process_status& status, abort_callback& abort) {
				status.set_item("Refreshing feed...");
				try {
					*ok = FetchOneChannel(url->get_ptr(), abort, *result);
					if (!*ok) *errMsg = "This URL doesn't look like a podcast RSS or Atom feed.";
				} catch (podcast_http::exception_http& e) {
					*errMsg = e.what();
				}
			},
			[result, ok, errMsg, onDone](fb2k::hwnd_t wnd, bool aborted) {
				if (aborted) return;
				if (!*ok) {
					popup_message::g_show(errMsg->get_ptr(), "Podcasts: could not refresh feed");
					return;
				}
				podcast::GetLibrary().merge_channel(*result);
				podcast::GetLibrary().save();
				if (onDone) onDone();
			}
		);
		threaded_process::g_run_modeless(cb, threaded_process::flag_show_abort | threaded_process::flag_show_item, parent, "Refreshing podcast");
	}

	void RefreshAll(fb2k::hwnd_t parent, std::function<void()> onDone) {
		auto feedUrls = std::make_shared<pfc::list_t<pfc::string8>>();
		for (t_size i = 0; i < podcast::GetLibrary().channels.get_count(); ++i) {
			feedUrls->add_item(podcast::GetLibrary().channels[i].feedUrl);
		}
		auto results = std::make_shared<std::vector<podcast::Channel>>();
		auto failures = std::make_shared<int>(0);

		auto cb = threaded_process_callback_lambda::create(
			[](fb2k::hwnd_t) {},
			[feedUrls, results, failures](threaded_process_status& status, abort_callback& abort) {
				for (t_size i = 0; i < feedUrls->get_count(); ++i) {
					abort.check();
					pfc::string8 msg = "Refreshing ";
					msg << (*feedUrls)[i];
					status.set_item(msg.get_ptr());
					status.set_progress(i, feedUrls->get_count());
					podcast::Channel ch;
					try {
						if (FetchOneChannel((*feedUrls)[i].get_ptr(), abort, ch)) {
							results->push_back(ch);
						} else {
							++(*failures);
						}
					} catch (podcast_http::exception_http&) {
						++(*failures);
					}
				}
			},
			[results, failures, onDone](fb2k::hwnd_t wnd, bool aborted) {
				for (auto& ch : *results) {
					podcast::GetLibrary().merge_channel(ch);
				}
				if (!results->empty()) podcast::GetLibrary().save();
				if (*failures > 0) {
					pfc::string8 msg;
					msg << *failures << " feed(s) could not be refreshed.";
					popup_message::g_show(msg.get_ptr(), "Podcasts");
				}
				if (onDone) onDone();
			}
		);
		threaded_process::g_run_modeless(cb, threaded_process::flag_show_abort | threaded_process::flag_show_item | threaded_process::flag_show_progress, parent, "Refreshing all podcasts");
	}

	void ImportOpmlFile(const char* path, fb2k::hwnd_t parent, std::function<void()> onDone) {
		auto filePath = std::make_shared<pfc::string8>(path);
		auto results = std::make_shared<std::vector<podcast::Channel>>();
		auto failures = std::make_shared<int>(0);
		auto parseError = std::make_shared<bool>(false);

		auto cb = threaded_process_callback_lambda::create(
			[](fb2k::hwnd_t) {},
			[filePath, results, failures, parseError](threaded_process_status& status, abort_callback& abort) {
				status.set_item("Reading OPML file...");
				pfc::string8 opmlText = ReadLocalFile(filePath->get_ptr());
				if (opmlText.is_empty()) { *parseError = true; return; }

				pfc::list_t<opml::FeedRef> feeds;
				opml::ParseOpml(opmlText, feeds);
				if (feeds.get_count() == 0) { *parseError = true; return; }

				for (t_size i = 0; i < feeds.get_count(); ++i) {
					abort.check();
					pfc::string8 msg = "Fetching ";
					msg << feeds[i].title;
					status.set_item(msg.get_ptr());
					status.set_progress(i, feeds.get_count());
					podcast::Channel ch;
					try {
						if (FetchOneChannel(feeds[i].url.get_ptr(), abort, ch)) {
							results->push_back(ch);
						} else {
							++(*failures);
						}
					} catch (podcast_http::exception_http&) {
						++(*failures);
					}
				}
			},
			[results, failures, parseError, onDone](fb2k::hwnd_t wnd, bool aborted) {
				if (aborted) return;
				if (*parseError) {
					popup_message::g_show("Could not read this file as an OPML podcast list.", "Podcasts: import failed");
					return;
				}
				for (auto& ch : *results) {
					podcast::GetLibrary().merge_channel(ch);
				}
				if (!results->empty()) podcast::GetLibrary().save();
				if (*failures > 0) {
					pfc::string8 msg;
					msg << *failures << " feed(s) listed in the OPML file could not be fetched.";
					popup_message::g_show(msg.get_ptr(), "Podcasts");
				}
				if (onDone) onDone();
			}
		);
		threaded_process::g_run_modeless(cb, threaded_process::flag_show_abort | threaded_process::flag_show_item | threaded_process::flag_show_progress, parent, "Importing OPML");
	}

	bool ExportOpmlFile(const char* path) {
		pfc::string8 xml = opml::BuildOpml(podcast::GetLibrary());
		return WriteLocalFile(path, xml);
	}

}
