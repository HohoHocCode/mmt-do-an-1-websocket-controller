import { withConnection } from "./db";
import { AuthenticatedUser } from "./types";
import { generateSetupToken, hashPassword } from "./authService";

type Args = Record<string, string | boolean>;

function parseArgs(): Args {
  const args: Args = {};
  for (const arg of process.argv.slice(2)) {
    if (arg.startsWith("--")) {
      const [key, value] = arg.replace(/^--/, "").split("=");
      args[key] = value ?? true;
    }
  }
  return args;
}

async function run() {
  await import("./migrate");
  const args = parseArgs();
  const username = String(args.username || args.u || "").trim();
  const role = (args.role || "user") as AuthenticatedUser["role"];
  const password = args.password ? String(args.password) : "";

  if (!username) {
    console.error("Usage: npm run create-user -- --username alice --role user|admin [--password secret]");
    process.exit(1);
  }
  if (role !== "admin" && role !== "user") {
    console.error("role must be 'admin' or 'user'");
    process.exit(1);
  }

  const existing = await withConnection(async (conn) => {
    const [rows] = await conn.query("SELECT id FROM users WHERE username = ?", [username]);
    return (rows as any[])[0];
  });
  if (existing) {
    console.error(`User '${username}' already exists (id=${existing.id}).`);
    process.exit(1);
  }

  const setup = password ? null : generateSetupToken();
  const passwordHash = password ? await hashPassword(password) : null;

  await withConnection(async (conn) => {
    await conn.query(
      `INSERT INTO users (username, role, password_hash, setup_token_hash, setup_token_created_at)
       VALUES (?, ?, ?, ?, ?)`,
      [username, role, passwordHash, setup ? await setup.hashPromise : null, setup ? setup.createdAt : null]
    );
  });

  if (setup) {
    console.log(`[create-user] Setup token for ${username}: ${setup.raw}`);
  } else {
    console.log(`[create-user] Created ${username} with password.`);
  }
}

run().catch((err) => {
  console.error("[create-user] Failed:", err);
  process.exit(1);
});
