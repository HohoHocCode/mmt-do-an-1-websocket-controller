import { withConnection } from "./db";
import { config } from "./config";
import { AuthenticatedUser } from "./types";
import { generateSetupToken, hashPassword } from "./authService";

async function upsertUser(
  username: string,
  role: AuthenticatedUser["role"],
  options: { password?: string }
) {
  const existing = await withConnection(async (conn) => {
    const [rows] = await conn.query("SELECT * FROM users WHERE username = ?", [username]);
    return (rows as any[])[0] as any | undefined;
  });

  if (existing) {
    const updates: string[] = [];
    const params: any[] = [];

    if (existing.role !== role) {
      updates.push("role = ?");
      params.push(role);
    }

    if (options.password) {
      updates.push("password_hash = ?");
      params.push(await hashPassword(options.password));
      updates.push("setup_token_hash = NULL, setup_token_created_at = NULL");
    } else if (!existing.password_hash) {
      const setup = generateSetupToken();
      updates.push("setup_token_hash = ?");
      params.push(await setup.hashPromise);
      updates.push("setup_token_created_at = ?");
      params.push(setup.createdAt);
      console.log(`[seed] Setup token for ${username}: ${setup.raw}`);
    }

    if (updates.length > 0) {
      params.push(existing.id);
      const sql = `UPDATE users SET ${updates.join(", ")} WHERE id = ?`;
      await withConnection((conn) => conn.query(sql, params));
    }
    return;
  }

  const setup = options.password ? null : generateSetupToken();
  const passwordHash = options.password ? await hashPassword(options.password) : null;
  await withConnection(async (conn) => {
    await conn.query(
      `INSERT INTO users (username, role, password_hash, setup_token_hash, setup_token_created_at)
       VALUES (?, ?, ?, ?, ?)`,
      [
        username,
        role,
        passwordHash,
        setup ? await setup.hashPromise : null,
        setup ? setup.createdAt : null,
      ]
    );
  });

  if (setup) {
    console.log(`[seed] Setup token for ${username}: ${setup.raw}`);
  } else {
    console.log(`[seed] Seeded ${username} with password`);
  }
}

async function runSeed() {
  console.log("[seed] Using DB", config.MYSQL_DATABASE);
  const adminPassword = process.env.ADMIN_SEED_PASSWORD || "";
  const demoPassword = process.env.DEMO_SEED_PASSWORD || "";

  await upsertUser("admin", "admin", { password: adminPassword || undefined });
  await upsertUser("demo", "user", { password: demoPassword || undefined });

  console.log("[seed] Done.");
}

// Ensure migrations are applied before seeding
await import("./migrate");
runSeed().catch((err) => {
  console.error("[seed] Failed:", err);
  process.exit(1);
});
