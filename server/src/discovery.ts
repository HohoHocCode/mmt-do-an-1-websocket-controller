import crypto from "crypto";
import dgram from "dgram";
import { config } from "./config";
import { DiscoveryResult } from "./types";

const DISCOVERY_PREFIX = "MMT_DISCOVER";
const MAX_MESSAGE_SIZE = 4096;

export interface DiscoveryOptions {
  timeoutMs?: number;
  retries?: number;
  port?: number;
}

function parsePayload(raw: Buffer) {
  const slice = raw.subarray(0, MAX_MESSAGE_SIZE);
  const text = slice.toString().trim();
  if (!text.length) return null;

  try {
    return JSON.parse(text) as Record<string, unknown>;
  } catch {
    return { text };
  }
}

function cleanResult(raw: any, address: string, nonce: string): DiscoveryResult | null {
  const wsPort =
    typeof raw.wsPort === "number"
      ? raw.wsPort
      : typeof raw.port === "number"
      ? raw.port
      : null;

  const name = typeof raw.name === "string" ? raw.name : undefined;
  const version = typeof raw.version === "string" ? raw.version : undefined;
  const responseNonce =
    typeof raw.nonce === "string"
      ? raw.nonce
      : typeof raw.nonce === "number"
      ? String(raw.nonce)
      : undefined;

  if (responseNonce && responseNonce !== nonce) return null;

  return {
    ip: typeof raw.ip === "string" ? raw.ip : address,
    wsPort,
    name,
    version,
    nonce: responseNonce,
    lastSeenMs: Date.now(),
  };
}

export async function discoverOnLan(opts: DiscoveryOptions = {}): Promise<DiscoveryResult[]> {
  const timeoutMs = Math.min(Math.max(opts.timeoutMs ?? config.DISCOVERY_TIMEOUT_MS, 500), 5000);
  const retries = Math.min(Math.max(opts.retries ?? config.DISCOVERY_RETRIES, 1), 5);
  const port = opts.port ?? config.DISCOVERY_PORT;
  const nonce = crypto.randomBytes(8).toString("hex");
  const message = Buffer.from(`${DISCOVERY_PREFIX} ${nonce}`);

  const socket = dgram.createSocket({ type: "udp4", reuseAddr: true });
  const results = new Map<string, DiscoveryResult>();

  return new Promise((resolve) => {
    const finish = () => {
      try {
        socket.close();
      } catch {
        // ignore
      }
      resolve(Array.from(results.values()));
    };

    socket.on("message", (msg, rinfo) => {
      const payload = parsePayload(msg);
      if (!payload || typeof payload !== "object") return;

      const cleaned = cleanResult(payload, rinfo.address, nonce);
      if (!cleaned) return;

      const key = `${cleaned.ip}:${cleaned.wsPort ?? "na"}`;
      results.set(key, cleaned);
    });

    socket.on("error", () => finish());

    socket.bind(() => {
      try {
        socket.setBroadcast(true);
      } catch {
        // ignore broadcast flag errors
      }

      const attempt = (current: number) => {
        if (current > retries) {
          setTimeout(finish, timeoutMs);
          return;
        }
        try {
          socket.send(message, 0, message.length, port, "255.255.255.255");
        } catch {
          setTimeout(finish, 10);
          return;
        }
        setTimeout(() => attempt(current + 1), timeoutMs);
      };

      attempt(1);
      setTimeout(finish, timeoutMs * (retries + 1));
    });
  });
}
