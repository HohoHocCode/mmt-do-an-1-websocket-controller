export type UserRole = "admin" | "user";

export interface UserRow {
  id: number;
  username: string;
  role: UserRole;
  password_hash: string | null;
  setup_token_hash: string | null;
  setup_token_created_at: Date | null;
  created_at: Date;
  updated_at: Date;
}

export interface AuthenticatedUser {
  id: number;
  username: string;
  role: UserRole;
}
