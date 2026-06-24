#include "stdafx.h"
#include "model.h"

namespace podcast {

	t_size Library::find_channel(const char* feedUrl) const {
		for (t_size i = 0; i < channels.get_count(); ++i) {
			if (strcmp(channels[i].feedUrl.get_ptr(), feedUrl) == 0) return i;
		}
		return pfc::infinite_size;
	}

	t_size Library::find_episode(const Channel& ch, const char* guid) const {
		for (t_size i = 0; i < ch.episodes.get_count(); ++i) {
			if (strcmp(ch.episodes[i].guid.get_ptr(), guid) == 0) return i;
		}
		return pfc::infinite_size;
	}

	void Library::merge_channel(const Channel& fresh) {
		t_size idx = find_channel(fresh.feedUrl.get_ptr());
		if (idx == pfc::infinite_size) {
			channels.add_item(fresh);
			return;
		}

		Channel& existing = channels[idx];
		existing.title = fresh.title;

		for (t_size i = 0; i < fresh.episodes.get_count(); ++i) {
			Episode ep = fresh.episodes[i];
			t_size oldIdx = find_episode(existing, ep.guid.get_ptr());
			if (oldIdx != pfc::infinite_size) {
				ep.listened = existing.episodes[oldIdx].listened;
			}
			if (oldIdx != pfc::infinite_size) {
				existing.episodes[oldIdx] = ep;
			} else {
				existing.episodes.add_item(ep);
			}
		}
	}

	void Library::remove_channel(t_size index) {
		channels.remove_by_idx(index);
	}

	pfc::string8 Library::storage_path() const {
		wchar_t appData[MAX_PATH];
		DWORD n = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
		pfc::string8 p;
		if (n > 0 && n < MAX_PATH) {
			p = pfc::stringcvt::string_utf8_from_wide(appData);
		}
		p << "\\foobar2000-v2\\foo_podcast_library.txt";
		return p;
	}

	static pfc::string8 escape_field(const char* s) {
		pfc::string8 out;
		for (const char* p = s; *p; ++p) {
			if (*p == '\t' || *p == '\n' || *p == '\r') out.add_char(' ');
			else out.add_char(*p);
		}
		return out;
	}

	static void split_tabs(const pfc::string8& line, pfc::list_t<pfc::string8>& out) {
		const char* base = line.get_ptr();
		t_size pos = 0, len = line.length();
		while (true) {
			t_size tab = line.find_first('\t', pos);
			t_size end = (tab == pfc::infinite_size) ? len : tab;
			pfc::string8 field;
			field.set_string(base + pos, end - pos);
			out.add_item(field);
			if (tab == pfc::infinite_size) break;
			pos = tab + 1;
		}
	}

	void Library::save() const {
		pfc::string8 path = storage_path();
		pfc::stringcvt::string_wide_from_utf8 pathW(path.get_ptr());

		pfc::string8 buf;
		for (t_size c = 0; c < channels.get_count(); ++c) {
			const Channel& ch = channels[c];
			buf << "C\t" << escape_field(ch.feedUrl.get_ptr()) << "\t" << escape_field(ch.title.get_ptr()) << "\r\n";
			for (t_size e = 0; e < ch.episodes.get_count(); ++e) {
				const Episode& ep = ch.episodes[e];
				buf << "E\t" << escape_field(ep.guid.get_ptr()) << "\t" << escape_field(ep.title.get_ptr())
					<< "\t" << escape_field(ep.url.get_ptr()) << "\t" << (int)ep.pubDate
					<< "\t" << (int)ep.durationSeconds << "\t" << (ep.listened ? 1 : 0) << "\r\n";
			}
		}

		HANDLE f = CreateFileW(pathW.get_ptr(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (f == INVALID_HANDLE_VALUE) return;
		DWORD written = 0;
		WriteFile(f, buf.get_ptr(), (DWORD)buf.length(), &written, nullptr);
		CloseHandle(f);
	}

	void Library::load() {
		channels.remove_all();

		pfc::string8 path = storage_path();
		pfc::stringcvt::string_wide_from_utf8 pathW(path.get_ptr());

		HANDLE f = CreateFileW(pathW.get_ptr(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (f == INVALID_HANDLE_VALUE) return;

		DWORD size = GetFileSize(f, nullptr);
		pfc::array_t<char> data;
		data.set_size(size);
		DWORD readBytes = 0;
		ReadFile(f, data.get_ptr(), size, &readBytes, nullptr);
		CloseHandle(f);

		pfc::string8 text;
		text.set_string(data.get_ptr(), readBytes);

		Channel* current = nullptr;
		t_size pos = 0;
		while (pos <= text.length()) {
			t_size eol = text.find_first('\n', pos);
			t_size end = (eol == pfc::infinite_size) ? text.length() : eol;
			t_size lineEnd = end;
			if (lineEnd > pos && text.get_ptr()[lineEnd - 1] == '\r') --lineEnd;

			pfc::string8 line;
			line.set_string(text.get_ptr() + pos, lineEnd - pos);

			if (line.length() > 0) {
				pfc::list_t<pfc::string8> fields;
				split_tabs(line, fields);
				if (fields.get_count() > 0) {
					if (strcmp(fields[0].get_ptr(), "C") == 0 && fields.get_count() >= 3) {
						Channel ch;
						ch.feedUrl = fields[1];
						ch.title = fields[2];
						channels.add_item(ch);
						current = &channels[channels.get_count() - 1];
					} else if (strcmp(fields[0].get_ptr(), "E") == 0 && fields.get_count() >= 7 && current != nullptr) {
						Episode ep;
						ep.guid = fields[1];
						ep.title = fields[2];
						ep.url = fields[3];
						ep.pubDate = (time_t)atoi(fields[4].get_ptr());
						ep.durationSeconds = atoi(fields[5].get_ptr());
						ep.listened = atoi(fields[6].get_ptr()) != 0;
						current->episodes.add_item(ep);
					}
				}
			}

			if (eol == pfc::infinite_size) break;
			pos = eol + 1;
		}
	}

	Library& GetLibrary() {
		static Library instance;
		static bool loaded = false;
		if (!loaded) {
			instance.load();
			loaded = true;
		}
		return instance;
	}

}
