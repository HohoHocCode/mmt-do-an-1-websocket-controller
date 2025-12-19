import { createPool, Pool, PoolConnection } from "mysql2/promise";
import { config } from "./config";

const pool: Pool = createPool({
  host: config.MYSQL_HOST,
  port: config.MYSQL_PORT,
  user: config.MYSQL_USER,
  password: config.MYSQL_PASSWORD,
  database: config.MYSQL_DATABASE,
  waitForConnections: true,
  connectionLimit: 10,
  namedPlaceholders: true,
});

export async function withConnection<T>(fn: (conn: PoolConnection) => Promise<T>): Promise<T> {
  const conn = await pool.getConnection();
  try {
    return await fn(conn);
  } finally {
    conn.release();
  }
}

export { pool };
