import { WebSocketServer } from "ws";
import crypto from "node:crypto";

const PORT = Number(process.env.PORT || 8080);
const MAX_PLAYERS = 4;
const rooms = new Map();

function send(ws, type, payload = {}) {
  if (ws.readyState === ws.OPEN) ws.send(JSON.stringify({ type, ...payload }));
}

function roomSnapshot(room) {
  return {
    code: room.code,
    players: [...room.players.values()].map(p => ({
      id: p.id,
      slot: p.slot,
      name: p.name,
      ready: p.ready
    }))
  };
}

function broadcast(room, type, payload = {}) {
  for (const p of room.players.values()) send(p.ws, type, payload);
}

function makeCode() {
  const alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for (;;) {
    let code = "";
    const bytes = crypto.randomBytes(6);
    for (let i = 0; i < 6; i++) code += alphabet[bytes[i] % alphabet.length];
    if (!rooms.has(code)) return code;
  }
}

function nextSlot(room) {
  for (let i = 0; i < MAX_PLAYERS; i++) {
    if (![...room.players.values()].some(p => p.slot === i)) return i;
  }
  return -1;
}

function leave(ws) {
  const code = ws.roomCode;
  if (!code) return;
  const room = rooms.get(code);
  if (!room) return;
  room.players.delete(ws.playerId);
  if (room.players.size === 0) {
    rooms.delete(code);
  } else {
    broadcast(room, "room_state", { room: roomSnapshot(room) });
  }
  ws.roomCode = null;
}

const wss = new WebSocketServer({ port: PORT });

wss.on("connection", ws => {
  ws.playerId = crypto.randomUUID();
  send(ws, "hello", { playerId: ws.playerId, protocol: 1 });

  ws.on("message", raw => {
    let msg;
    try { msg = JSON.parse(raw.toString()); }
    catch { return send(ws, "error", { message: "invalid_json" }); }

    if (msg.type === "create_room") {
      leave(ws);
      const code = makeCode();
      const room = { code, players: new Map(), started: false };
      const player = { id: ws.playerId, ws, slot: 0, name: String(msg.name || "Player 1").slice(0, 24), ready: false };
      room.players.set(player.id, player);
      rooms.set(code, room);
      ws.roomCode = code;
      return send(ws, "room_state", { room: roomSnapshot(room), host: true });
    }

    if (msg.type === "join_room") {
      leave(ws);
      const code = String(msg.code || "").toUpperCase();
      const room = rooms.get(code);
      if (!room) return send(ws, "error", { message: "room_not_found" });
      if (room.started) return send(ws, "error", { message: "room_started" });
      const slot = nextSlot(room);
      if (slot < 0) return send(ws, "error", { message: "room_full" });
      const player = { id: ws.playerId, ws, slot, name: String(msg.name || `Player ${slot + 1}`).slice(0, 24), ready: false };
      room.players.set(player.id, player);
      ws.roomCode = code;
      return broadcast(room, "room_state", { room: roomSnapshot(room) });
    }

    const room = rooms.get(ws.roomCode);
    if (!room) return send(ws, "error", { message: "not_in_room" });
    const player = room.players.get(ws.playerId);
    if (!player) return;

    if (msg.type === "ready") {
      player.ready = Boolean(msg.ready);
      return broadcast(room, "room_state", { room: roomSnapshot(room) });
    }

    if (msg.type === "start") {
      const host = [...room.players.values()].find(p => p.slot === 0);
      if (!host || host.id !== ws.playerId) return send(ws, "error", { message: "host_only" });
      if (room.players.size < 2) return send(ws, "error", { message: "need_two_players" });
      if ([...room.players.values()].some(p => !p.ready && p.id !== host.id)) {
        return send(ws, "error", { message: "players_not_ready" });
      }
      room.started = true;
      const seed = crypto.randomBytes(4).readUInt32BE(0);
      return broadcast(room, "game_start", { seed, players: roomSnapshot(room).players });
    }

    if (msg.type === "input") {
      if (!room.started) return;
      const frame = Number(msg.frame);
      if (!Number.isInteger(frame) || frame < 0) return;
      return broadcast(room, "input", {
        playerId: ws.playerId,
        slot: player.slot,
        frame,
        buttons: Number(msg.buttons) >>> 0,
        stickX: Math.max(-1, Math.min(1, Number(msg.stickX) || 0)),
        stickY: Math.max(-1, Math.min(1, Number(msg.stickY) || 0))
      });
    }

    if (msg.type === "state_hash") {
      return broadcast(room, "state_hash", {
        playerId: ws.playerId,
        frame: Number(msg.frame) || 0,
        hash: String(msg.hash || "").slice(0, 128)
      });
    }
  });

  ws.on("close", () => leave(ws));
  ws.on("error", () => leave(ws));
});

console.log(`Mario Party 1 netplay lobby server listening on :${PORT}`);
