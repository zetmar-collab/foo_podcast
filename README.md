# foo_podcast

A native foobar2000 input plugin providing a podcast subscription library.

## Features

- A dockable "Podcasts" panel (View > Podcasts) listing every subscribed channel as a tree, with episodes nested underneath each channel.
- Add a single podcast by its RSS/Atom feed URL.
- Import subscriptions from an OPML file, and export the current subscription list to OPML.
- Play any episode directly (double-click, or right-click > Play) — episodes stream straight from their original URL through foobar2000's built-in HTTP input, no extra tools required.
- Mark episodes as listened / unlistened; unlistened counts show next to each channel.
- Sort episodes within each channel by newest first, oldest first, or by name.
- Refresh a single podcast or all subscriptions at once.

## Installation

1. Download the latest `foo_podcast-vX.Y.fb2k-component` file from the [Releases](https://github.com/zetmar-collab/foo_podcast/releases) page.
2. In foobar2000, open **File > Preferences > Components > Install...** and select the downloaded `.fb2k-component` file.
3. Confirm the restart prompt.
4. Open **View > Podcasts** to show the panel, then right-click inside it to add a podcast by URL or import an OPML file.

## Building from source

Requires:
- Visual Studio 2022 Build Tools (v143 toolset) with the VC/ATL component
- [foobar2000 SDK](https://www.foobar2000.org/SDK) extracted to `../sdk/` (sibling of this folder)
- [WTL](https://sourceforge.net/projects/wtl/) extracted to `../wtl/` (sibling of this folder)

Build `foo_podcast.sln` (not the bare `.vcxproj` — the solution maps `pfc` to the `Debug FB2K`/`Release FB2K` configurations required to avoid duplicate-symbol link errors).

Deploy the resulting `foo_podcast.dll` to foobar2000's `user-components-x64\foo_podcast\` folder (use the in-app **Install...** dialog once so foobar2000 registers the component).

## Notes

- Feed parsing uses the system's built-in MSXML library; fetching uses WinHTTP. No external dependencies are required at runtime.
- Subscriptions and listened state are stored in a plain text file under the foobar2000 profile folder (`foo_podcast_library.txt`).
