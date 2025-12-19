import fs from "fs";
import path from "path";
import dotenv from "dotenv";
import { z } from "zod";

const candidateEnvPaths = [
  path.resolve(process.cwd(), ".env"),
  path.resolve(process.cwd(), "..", ".env"),
];

for (const envPath of candidateEnvPaths) {
  if (fs.existsSync(envPath)) {
    dotenv.config({ path: envPath });
    break;
  }
}

const envSchema = z.object({
  MYSQL_HOST: z.string().default("localhost"),
  MYSQL_PORT: z.coerce.number().int().positive().default(3306),
  MYSQL_USER: z.string().default("mmt"),
  MYSQL_PASSWORD: z.string().default("mmtpass"),
  MYSQL_DATABASE: z.string().default("mmt_controller"),
  JWT_SECRET: z.string().min(10, "JWT_SECRET must be set"),
  SETUP_TOKEN_SECRET: z.string().min(10, "SETUP_TOKEN_SECRET must be set"),
  AUTH_TOKEN_TTL_SECONDS: z.coerce.number().int().positive().default(86400),
  HOST: z.string().default("127.0.0.1"),
  PORT: z.coerce.number().int().positive().default(8080),
  DISCOVERY_PORT: z.coerce.number().int().positive().default(41000),
  DISCOVERY_TIMEOUT_MS: z.coerce.number().int().positive().default(1800),
  DISCOVERY_RETRIES: z.coerce.number().int().positive().max(5).default(2),
  CONTROLLER_CMD: z.string().default(""),
  CONTROLLER_ARGS: z.string().default(""),
  CONTROLLER_WORKDIR: z.string().default(""),
  CONTROLLER_GRACE_MS: z.coerce.number().int().positive().default(1200),
  CONTROLLER_AUTO_START: z
    .union([z.literal("1"), z.literal("0"), z.literal("true"), z.literal("false"), z.literal(""), z.boolean()])
    .optional()
    .default("0"),
});

const envInput = {
  ...process.env,
  PORT: process.env.PORT ?? process.env.BACKEND_PORT,
};

export const config = envSchema.parse(envInput);
