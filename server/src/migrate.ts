import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { withConnection } from "./db";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

async function ensureMigrationsTable() {
  await withConnection(async (conn) => {
    await conn.query(`
      CREATE TABLE IF NOT EXISTS schema_migrations (
        id INT AUTO_INCREMENT PRIMARY KEY,
        name VARCHAR(255) NOT NULL UNIQUE,
        applied_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
      )
    `);
  });
}

async function getAppliedNames(): Promise<Set<string>> {
  const rows = await withConnection(async (conn) => {
    const [res] = await conn.query("SELECT name FROM schema_migrations");
    return res as { name: string }[];
  });
  return new Set(rows.map((r) => r.name));
}

async function applyMigration(name: string, sql: string) {
  await withConnection(async (conn) => {
    await conn.beginTransaction();
    try {
      await conn.query(sql);
      await conn.query("INSERT INTO schema_migrations (name) VALUES (?)", [name]);
      await conn.commit();
    } catch (err) {
      await conn.rollback();
      throw err;
    }
  });
}

async function runMigrations() {
  const migrationsDir = path.resolve(__dirname, "..", "migrations");
  await ensureMigrationsTable();
  const applied = await getAppliedNames();

  const files = fs
    .readdirSync(migrationsDir)
    .filter((f) => f.endsWith(".sql"))
    .sort();

  for (const file of files) {
    if (applied.has(file)) {
      console.log(`[migrate] Skip ${file} (already applied)`);
      continue;
    }
    const fullPath = path.join(migrationsDir, file);
    const sql = fs.readFileSync(fullPath, "utf-8");
    console.log(`[migrate] Applying ${file}...`);
    await applyMigration(file, sql);
    console.log(`[migrate] Applied ${file}`);
  }
  console.log("[migrate] Done");
}

runMigrations().catch((err) => {
  console.error("[migrate] Failed:", err);
  process.exit(1);
});
