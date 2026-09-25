# Mario Party 1 netplay server

Lightweight WebSocket lobby/signaling/input-relay foundation for the Android port.

Current protocol:
- create private room
- join by six-character room code
- up to four player slots
- ready state
- host start
- shared game seed
- frame-numbered input packets
- state-hash packets for desync detection

This server never receives or distributes ROM data.

Run locally:

```bash
cd server
npm install
npm start
```

Default port: `8080`. Set `PORT` to override it.

This is the lobby/input-relay foundation. The game runtime still needs deterministic frame synchronization and state-hash integration before online play is usable.
