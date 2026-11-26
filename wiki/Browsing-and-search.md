## The tabs

**Home · Movies · TV Shows · Live TV · VOD · Music · Discover**. A tab only appears when there is something in it: Live TV and VOD need a source with channels or titles, and Discover needs [Seerr](Seerr). **L1 / R1** move between tabs.

## Home

- Featured content, **Continue Watching**, **Next Up** and **Recently Added**, plus recommendations.
- Home follows the home sections you set on your server, including Emby 4.10 and later's. A section left unset takes the server's default, and a merged "keep watching" section shows Continue Watching and Next Up.
- Each row loads on its own: if one request fails, the others still show. **Options › Retry Home sections** tries again without restarting.
- **Options › View all in this row** opens the whole row as pages. For Recently Added, that's the library sorted by date added.

## Libraries

- Movies, TV shows with their seasons and episodes, and [music](Music).
- **Sort** and **filter** (unwatched only, favourites only, genre, decade) with **□**. Your choices are remembered per account and per library.
- **A–Z**: **L2 / R2** jump to the previous or next letter.
- Favourites and watched / unwatched status are kept in step with your server.
- Coming back from a details page or another tab returns you to the same title, and the selection survives a refresh while that title still exists.
- Big libraries stay quick: only the cards on screen are drawn.

## Details pages

- Artwork, the full description (**□** to read more), cast and crew, extras, more like this, and a season and episode picker.
- Upcoming episodes appear in a season's details.
- **Shuffle** on a show's page, beside Play, plays the whole show in random order, each episode from its start. Specials (season 0) are left out.
- **Seasons as posters** (**Settings › Playback › Show seasons as posters**, off by default): a show's page picks its season by poster instead of by name.
- **Included in** (**Settings › Playback › Show "Included in" on film pages**, off by default): a film's page shows the collections it's in, above More like this. The first film page opened reads your collections once in the background; a film newly added to a collection shows within ten minutes.
- **Trailer QR**: when the server has a link to a trailer, scan the code to watch it on your phone. Seerr isn't needed for this.
- **Options** has shortcuts where they apply: **Play / Resume**, **Play from beginning**, **Go to series**, **Go to season**, **Trailer QR**, mark as watched or unwatched. Marking a whole season or series asks you to confirm.

## Search

**△** searches the section you're in: the selected Movies or TV Shows library, the selected Music view, Discover, the loaded Live TV channels, or the selected VOD type. **○** clears or closes the search. **△ does nothing on Home**, and there is no separate Search tab.

## Artwork

Posters, backdrops, logos and thumbnails come from your server. From 1.2.7 Emby5 signs in for every picture the way it does for everything else, so servers that only show pictures to signed-in users (or sit behind a proxy that asks for sign-in) show artwork too. A title the server has no picture for keeps a placeholder.
