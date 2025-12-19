import cors from "cors";
import express, { NextFunction, Request, Response } from "express";
import { z } from "zod";
import { config } from "./config";
import { withConnection } from "./db";
import "./migrate";
import {
  generateJwt,
  hashPassword,
  verifyJwt,
  verifyPassword,
  verifySetupToken,
} from "./authService";
import { controllerManager } from "./controllerManager";
import { discoverOnLan } from "./discovery";
import { AuthenticatedUser, ControllerStatus, UserRow } from "./types";

const app = express();
app.use(cors());
app.use(express.json());

type AuthedRequest = Request & { user?: AuthenticatedUser };

function formatUser(row: any): UserRow {
  return {
    ...row,
    id: Number(row.id),
    setup_token_created_at: row.setup_token_created_at ? new Date(row.setup_token_created_at) : null,
    created_at: new Date(row.created_at),
    updated_at: new Date(row.updated_at),
  };
}

async function getUserByUsername(username: string): Promise<UserRow | null> {
  const rows = await withConnection(async (conn) => {
    const [result] = await conn.query("SELECT * FROM users WHERE username = ?", [username]);
    return result as any[];
  });
  if (!rows.length) return null;
  return formatUser(rows[0]);
}

async function audit(action: string, meta: Record<string, unknown>, user?: AuthenticatedUser | null, username?: string) {
  try {
    await withConnection((conn) =>
      conn.query(
        "INSERT INTO audit_log (user_id, username, action, meta_json) VALUES (?, ?, ?, ?)",
        [user?.id ?? null, username ?? user?.username ?? null, action, JSON.stringify(meta)]
      )
    );
  } catch (err) {
    console.error("[audit] Failed to record audit log:", err);
  }
}

function requireAuth(req: Request, res: Response, next: NextFunction) {
  const header = req.headers.authorization;
  if (!header || !header.startsWith("Bearer ")) {
    return res.status(401).json({ ok: false, error: "Unauthorized" });
  }
  const token = header.replace("Bearer ", "");
  const user = verifyJwt(token);
  if (!user) {
    return res.status(401).json({ ok: false, error: "Invalid token" });
  }
  (req as AuthedRequest).user = user;
  next();
}

function requireAdmin(req: Request, res: Response, next: NextFunction) {
  const user = (req as AuthedRequest).user;
  if (!user || user.role !== "admin") {
    return res.status(403).json({ ok: false, error: "Admin role required" });
  }
  next();
}

app.get("/auth/status", async (req, res) => {
  const schema = z.object({ username: z.string().min(1) });
  const parsed = schema.safeParse(req.query);
  if (!parsed.success) return res.status(400).json({ error: "username is required" });
  const username = String(parsed.data.username).trim();
  const user = await getUserByUsername(username);
  res.json({
    exists: !!user,
    hasPassword: !!user?.password_hash,
  });
});

app.post("/auth/login", async (req, res) => {
  const schema = z.object({
    username: z.string().min(1),
    password: z.string().min(4),
  });
  const parsed = schema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid payload" });
  const { username, password } = parsed.data;

  const user = await getUserByUsername(username);
  if (!user || !user.password_hash) {
    await audit("login", { success: false, reason: "missing_password_or_user" }, null, username);
    return res.status(400).json({ error: "Account not ready. Please set password first." });
  }

  const ok = await verifyPassword(password, user.password_hash);
  if (!ok) {
    await audit("login", { success: false, reason: "wrong_password" }, null, username);
    return res.status(401).json({ error: "Invalid username or password" });
  }

  const authUser: AuthenticatedUser = { id: user.id, username: user.username, role: user.role };
  const token = generateJwt(authUser);
  await audit("login", { success: true }, authUser);

  res.json({ token, user: authUser });
});

app.post("/auth/set-password", async (req, res) => {
  const schema = z.object({
    username: z.string().min(1),
    setupToken: z.string().min(8),
    newPassword: z.string().min(8),
  });
  const parsed = schema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid payload" });
  const { username, setupToken, newPassword } = parsed.data;

  const user = await getUserByUsername(username);
  if (!user) return res.status(404).json({ error: "User not found" });
  if (user.password_hash) return res.status(400).json({ error: "Password already set" });
  if (!user.setup_token_hash) return res.status(400).json({ error: "Setup token missing" });

  const tokenOk = await verifySetupToken(setupToken, user.setup_token_hash, user.setup_token_created_at);
  if (!tokenOk) {
    await audit("set_password", { success: false, reason: "setup_token_invalid" }, { id: user.id, username: user.username, role: user.role });
    return res.status(401).json({ error: "Invalid or expired setup token" });
  }

  const passwordHash = await hashPassword(newPassword);
  await withConnection(async (conn) => {
    await conn.query(
      "UPDATE users SET password_hash = ?, setup_token_hash = NULL, setup_token_created_at = NULL WHERE id = ?",
      [passwordHash, user.id]
    );
  });
  await audit("set_password", { success: true }, { id: user.id, username: user.username, role: user.role });
  res.json({ ok: true });
});

app.post("/auth/verify", async (req, res) => {
  const schema = z.object({ token: z.string().min(1) });
  const parsed = schema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ ok: false });
  const user = verifyJwt(parsed.data.token);
  if (!user) return res.json({ ok: false });

  const dbUser = await getUserByUsername(user.username);
  if (!dbUser) return res.json({ ok: false });

  res.json({ ok: true, user: { username: user.username, role: user.role } });
});

app.post("/audit", requireAuth, async (req: Request, res: Response) => {
  const schema = z.object({
    action: z.string().min(1),
    meta: z.record(z.any()).optional(),
  });
  const parsed = schema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json({ error: "Invalid payload" });
  const user = (req as AuthedRequest).user!;
  await audit(parsed.data.action, parsed.data.meta ?? {}, user);
  res.json({ ok: true });
});

app.post("/api/discover", requireAuth, async (req: Request, res: Response) => {
  const schema = z.object({
    timeoutMs: z.number().int().positive().max(5000).optional(),
    retries: z.number().int().min(1).max(5).optional(),
  });
  const parsed = schema.safeParse(req.body ?? {});
  if (!parsed.success) {
    return res.status(400).json({ ok: false, error: "Invalid payload" });
  }
  const devices = await discoverOnLan(parsed.data);
  res.json({ ok: true, devices });
});

app.get("/devices/discover", requireAuth, async (_req: Request, res: Response) => {
  const devices = await discoverOnLan();
  res.json({ ok: true, devices });
});

app.get("/api/controller/status", requireAuth, (_req: Request, res: Response) => {
  const status: ControllerStatus = controllerManager.getStatus();
  res.json({ ok: true, status });
});

app.post("/api/controller/restart", requireAuth, requireAdmin, async (_req: Request, res: Response) => {
  const status = await controllerManager.restart();
  if (status.status === "error") {
    return res.status(500).json({ ok: false, error: status.message ?? "Failed to restart", status });
  }
  res.json({ ok: true, status });
});

app.post("/api/controller/stop", requireAuth, requireAdmin, async (_req: Request, res: Response) => {
  const status = await controllerManager.stop();
  if (status.status === "error") {
    return res.status(500).json({ ok: false, error: status.message ?? "Stop failed", status });
  }
  res.json({ ok: true, status });
});

app.use((err: Error, _req: Request, res: Response, _next: NextFunction) => {
  console.error("[server] Unexpected error", err);
  res.status(500).json({ error: "Internal server error" });
});

app.listen(config.BACKEND_PORT, () => {
  console.log(`[server] listening on port ${config.BACKEND_PORT}`);
});

const shouldAutoStart =
  config.CONTROLLER_AUTO_START === true ||
  config.CONTROLLER_AUTO_START === "1" ||
  config.CONTROLLER_AUTO_START === "true";

if (shouldAutoStart) {
  controllerManager.start().catch((err) => {
    console.error("[controller] auto-start failed:", err);
  });
}
