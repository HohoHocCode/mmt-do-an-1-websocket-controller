import bcrypt from "bcrypt";
import crypto from "crypto";
import jwt from "jsonwebtoken";
import { config } from "./config";
import { AuthenticatedUser } from "./types";

const BCRYPT_ROUNDS = 10;
const SETUP_TOKEN_TTL_MS = 24 * 60 * 60 * 1000; // 24h

export async function hashPassword(password: string): Promise<string> {
  return bcrypt.hash(password, BCRYPT_ROUNDS);
}

export async function verifyPassword(password: string, hash: string): Promise<boolean> {
  return bcrypt.compare(password, hash);
}

export function generateJwt(user: AuthenticatedUser): string {
  return jwt.sign(
    { sub: user.id, username: user.username, role: user.role },
    config.JWT_SECRET,
    { expiresIn: config.AUTH_TOKEN_TTL_SECONDS }
  );
}

export function verifyJwt(token: string): AuthenticatedUser | null {
  try {
    const decoded = jwt.verify(token, config.JWT_SECRET) as jwt.JwtPayload;
    if (!decoded || typeof decoded !== "object") return null;
    if (!decoded.sub || !decoded.username || !decoded.role) return null;
    return {
      id: Number(decoded.sub),
      username: String(decoded.username),
      role: decoded.role as AuthenticatedUser["role"],
    };
  } catch {
    return null;
  }
}

export function generateSetupToken() {
  const raw = crypto.randomBytes(24).toString("hex");
  const salted = raw + config.SETUP_TOKEN_SECRET;
  const createdAt = new Date();
  return {
    raw,
    hashPromise: bcrypt.hash(salted, BCRYPT_ROUNDS),
    createdAt,
  };
}

export async function verifySetupToken(raw: string, storedHash: string, createdAt: Date | null): Promise<boolean> {
  if (!createdAt) return false;
  const age = Date.now() - createdAt.getTime();
  if (age < 0 || age > SETUP_TOKEN_TTL_MS) return false;
  const salted = raw + config.SETUP_TOKEN_SECRET;
  return bcrypt.compare(salted, storedHash);
}
