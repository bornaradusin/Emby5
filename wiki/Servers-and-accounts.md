Emby5 works with **Emby** and **Jellyfin**. Accounts from both can be saved side by side, and you switch between them on the **Who's watching?** screen.

## Connecting

- **On your network:** Emby5 looks for both kinds of server and lists what it finds under *Found on your network*.
- **By address:** enter `http://` or `https://` with the server's address, for example `192.168.0.10:8096`. Addresses behind a reverse proxy with a base path (such as `https://example.com/emby`) work too. A bare address gets port 8096.
- Whether a server is Emby or Jellyfin is read from the server's own public information, not from its name.

## Signing in

- **Username and password** on both Emby and Jellyfin.
- **Quick Connect** on Jellyfin, when the server has it turned on: Emby5 shows a code and a QR code. Open Jellyfin on your phone or computer, go to **Settings › Quick Connect**, and enter the code or scan it and authorize.

## Several users and servers

- Every account remembers its server and whether it is Emby or Jellyfin.
- Switching account or server never sends one server's sign-in to another.
- On **Who's watching?**, press **△** twice on an account to remove it from this PS5.
- Accounts saved by older versions keep working: they count as Emby and are checked when you sign in.

Your accounts and settings survive restarts and updates (see [where Emby5 keeps its data](Installing-and-updating#where-emby5-keeps-its-data)).

What plays, and how, also depends on how your server is set up and on the formats of your media.
