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

## Browsing and the guide

- **Left / Right** move along a row, **Up / Down** between rows. **✕** plays a channel, **△** searches the loaded channels by name, **○** leaves the search.
- When the source provides a guide (EPG), cards show what's on now and next, with times and progress, and the player shows the programme's details. There is no full guide grid.
- Coming back from playing or searching returns you to the same row and channel.
- Channel logos are used when the source has them; otherwise the name is shown.
- Channels found before may be shown from the cache, but playing one needs the stream to be available.

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
