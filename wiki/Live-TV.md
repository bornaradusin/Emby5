The **Live TV** tab shows channels as logo cards, one row per group, in the style of Home. The tab is hidden when no source has channels.

Emby5 contains no channels, subscriptions, playlists or credentials. Use sources you are entitled to.

## Sources

Set them up in **Settings › Live TV**:

- **Emby or Jellyfin Live TV**: the channels your server provides, including M3U tuners set up on the server.
- **Xtream Codes** providers: server address, username and password.
- **M3U / M3U8** playlist addresses. A single M3U8 (HLS) address can be one stream rather than a list of channels.

Channels from all sources appear together, each still playing through its own source. Status and retry for each source are in **Settings › Live TV**.

## Groups

- Rows come from the playlist's `group-title` or the Xtream category names (for example *Brazil, USA, Sports*). Nothing is built in.
- **Your own groups come first**, in the order you saved them, then the providers' groups. A channel can stay in its original group as well.
- Manage your groups, which channels are in them and their order in **Settings › Live TV › Manage IPTV categories**. Hold Up/Down to move faster through the list.
- **Copy a provider's group into one of yours**: select your group, press **Options**, choose **Add channels from a provider group** and confirm. Channels already in your group keep their place, duplicates are skipped and new channels go at the end. It's a one-time copy, not a link, and the provider's group is left as it is.
- **Options › Jump to group** opens a searchable list of groups. **Pin / unpin current group** adds shortcuts without changing the order.

## Browsing

- **Left / Right** move along a row, **Up / Down** between rows. **✕** plays a channel, **△** searches the loaded channels by name, **○** leaves the search, **□** opens the [TV guide](#the-tv-guide).
- When there's a guide (EPG) for a channel, its card shows what's on now and next, and the player shows the programme's details.
- Coming back from playing or searching returns you to the same row and channel.
- Channel logos are used when the source has them; otherwise the name is shown.
- Channels found before may be shown from the cache, but playing one needs the stream to be available.

## The TV guide

Press **□** in Live TV for the guide: the channels of the group down the left, the time across from now to three hours ahead, a block for each programme and a line at the current time. The focused programme's title, time, channel, how long is left and its description are at the top. Search results (**△**) have a guide too.

| Button | |
| --- | --- |
| **Up / Down** | the previous / next channel (hold to go faster) |
| **Left / Right** | the previous / next programme on that channel, up to about 12 hours ahead |
| **L1 / R1** | the previous / next group |
| **✕** | watch the channel |
| **□** or **○** | back to the channel cards |
| **Options** | jump to a group, pinned groups |

Coming back from watching returns you to the guide, on the channel you watched last. Programmes in the past aren't shown, and there's no recording, reminders or catch-up.

Where the guide comes from:

- **Emby and Jellyfin**: the server's own guide, a few channels at a time as you scroll.
- **Xtream**: the provider's guide, one channel at a time as you scroll, so nothing big is downloaded.
- **M3U**: an XMLTV guide file. Emby5 uses the one the playlist names (`url-tvg` or `x-tvg-url` in its first line), or the one you enter in **Settings › Live TV › M3U TV guide (XMLTV) URL**. Plain `.xml` and gzip-compressed `.xml.gz` both work. It's downloaded in the background and kept for 12 hours, so the next start is quick. Channels are matched by their `tvg-id`, or else by name. Only the next 14 hours or so are kept, and at most 80,000 programmes, so a huge guide can't fill the console's memory.

When a channel has no guide, its row says so and you can still watch it.

## Switching channels while watching

With the controls hidden in full-screen Live TV:

| Button | |
| --- | --- |
| **Left / Right** | previous / next channel in this group |
| **Up / Down** | first channel of the previous / next group |
| **Options** | playback controls (**○** hides them) |

Groups are in the order shown (yours first), empty groups are skipped, and both wrap around at the ends. One press is one switch: holding a button doesn't skip through several streams. The previous stream is closed properly before the next one opens.

## Big catalogues

Xtream channels arrive category by category, and each checked batch can be used before the whole refresh is done. A transfer that's still moving isn't cut off, only one that stalls. Big refreshes pause while you watch and carry on afterwards. If one category fails, the others still work and the cached channels stay; a complete refresh replaces what's gone.
