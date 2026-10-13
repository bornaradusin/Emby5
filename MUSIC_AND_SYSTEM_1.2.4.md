# Emby5 1.2.4 - Music artwork and System settings

This source revision addresses album-cover arrival and the agreed System layout.
It does not change the public version, themes, playback engine, GPU allocator,
Xtream catalogue handling, watched/favourite protocol, data directory or native
notification mechanism. It is a source update for CT108 and PS5 testing.

## Music artwork

- Library and album screens expose a guarded data revision. The UI loop polls
  this even while idle, so completed background metadata requests can trigger a
  new frame without pressing a button or switching tabs. Image downloads and GPU
  uploads continue using the existing artwork renderer and image workers.
- The music grid resolves metadata for visible albums, with the focused album
  first, using at most one dedicated metadata IO worker per Library instance.
  It no longer fetches album detail and tracks for every item on an entire page.
- Album primary artwork is preferred. A valid untagged album image is not replaced
  merely because a listing omitted its tag. Track/album artwork alternatives are
  considered after the preferred image has failed. Missing covers use a placeholder.
- Resolved cover information is independent of the abbreviated list returned by
  watched-data/background refreshes. An explicit changed image tag supersedes the
  previous result. Generation and lifetime checks drop results from replaced
  sources/screens; new server/account screens do not share the metadata cache.
- Transient metadata errors have bounded retry spacing (5/10/15 seconds after
  successive failures), at most three attempts until reopening the tab allows
  another attempt. No draw-thread network requests or global cache deletion.
- The album detail page also publishes cover/track updates independently and can
  receive a later track-list refresh instead of retaining only its first copy.

## System page

```
Settings -> System
    Appearance
    What's New
    Check for updates       On / Off
    Installed version      1.2.4
    Connected server       current product/name/version or base address
    Persistent storage     /data/Emby5/
```

The About option/page and its credits row have been removed from Settings.
Connected server and Persistent storage are read-only. Source licences and
attribution documents remain. Appearance, Playback, provider/group settings and
VOD-at-the-bottom-of-Live-TV are otherwise unchanged.

## Release notification preference

- `checkUpdates` is a local boolean in the existing settings JSON. It defaults to
  true for a fresh or missing/malformed legacy field. A saved false remains false.
  Appearance Apply/Cancel/reset does not change it.
- On permits one automatic check after sign-in. Enabling it while already signed
  in also permits a new check without restarting. Multiple account/sign-in calls
  coalesce; there is at most one network request in flight.
- Off prevents automatic requests, removes queued notices and invalidates notice
  permission for an already-running request. An in-flight read may finish its
  existing bounded HTTP request, but cannot display a stale notification afterward.
- Manual What's New still fetches read-only release information while Off, without
  changing the preference. Manual checks do not produce update-available popups.
- A successful automatic result for a newer stable version is delivered through
  the unchanged main-thread `notify()` / `sceKernelSendNotificationRequest` path.
  The same newer numeric version is notified at most once per application launch.
  HTTP failures, malformed releases and up-to-date results do not generate a popup.
- Failed reads preserve previous valid notes. The notes page provides status and
  can retry by closing/reopening. No asset download, install, launch or privilege
  helper is added. Normal playback/network error notifications are unaffected.

## Regression coverage

Run `bash tests/host/run_124_regressions.sh`. The suite now contains 34 executable
host tests and integration syntax checks. New tests exercise the actual library
and album code with delayed metadata, no-input redraw, visible-first scheduling,
primary/track preference, missing covers, changed tags, refresh retention, bounded
retry and stale source/account completion, for both Emby and Jellyfin routes.

The update-service test exercises 21 controlled HTTP requests: manual access with
Off, coalescing, enable, disabling an in-flight/queued notification, Off/On races,
version-normalised duplicate prevention, current/malformed/draft/prerelease
responses, HTTP failures and exception cleanup. Settings tests cover the six-row
System page, read-only rows, manual notes while Off, immediate toggle callbacks,
legacy/default/false persistence and appearance-reset isolation.

Network, image/rendering and PS5 platform functions in these tests are controlled
host dependencies, not a live Emby/Jellyfin server or PS5. Syntax checks are not a
native compile. Full native build and actual console behaviour remain to be tested.

## Build and console checklist

Keep both source archives in `/mnt/media/builder/archives/Emby5/`. The v1.2.4 ZIP
reuses the ten unchanged local font assets from v1.2.3 via the existing checked
restorer; it does not redistribute them. Extract a fresh workspace and run the
included `scripts/build-ct108.sh`. Successful native verification copies the
compiled package to `/mnt/media/builder/releases/Emby5/1.2.4/Emby5-1.2.4.zip`, after
backing up a previous 1.2.4 test build. The build script and v1.2.3 are unchanged.

On PS5, back up the current installation and `/data/Emby5/` and preserve that data
directory during installation. Verify:

1. Open Music -> Albums immediately after launch; wait without pressing a button.
   Covers should appear as they load. Test new/uncached albums, missing artwork,
   scrolling, Artists/Albums switching and opening an album on Emby and Jellyfin.
2. Change watched/favourite data or return after playback. Resolved covers should
   not disappear merely because the library listing refreshed. Change servers or
   users and check that no prior user's artwork/list state is published.
3. System contains exactly the six rows above, with no About page. Server info
   follows the connected server; storage is `/data/Emby5/` and cannot be edited.
4. Turn Check for updates Off, fully relaunch, and verify it remains Off. What's New
   can still be read manually. No automatic update notification should appear.
5. With a genuinely newer stable release available, test On and check for one
   kernel notification. Switch accounts and toggle Off/On; the same version should
   not repeatedly notify in that app launch. Also test disabling during a check.
6. Recheck existing themes, Live TV controls, Xtream, watched/favourite actions,
   video/audio playback and upscaling. This revision does not change or newly
   certify GPU or stalled-provider cancellation behaviour.
