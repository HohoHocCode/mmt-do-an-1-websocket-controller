import { createContext, useCallback, useContext, useMemo, useState } from "react";
import type { PropsWithChildren } from "react";
import type { AuthUser } from "./types";
import { verifyOrRegisterUser } from "./user_db";

type AuthContextValue = {
  user: AuthUser | null;
  token: string | null;
  loading: boolean;
  locked: boolean;
  lockedReason?: string;
  login: (username: string, password: string) => Promise<void>;
  logout: () => void;
  clearLock: () => void;
  lockSession: (reason?: string) => void;
  setSession: (token: string, user: AuthUser) => void;
};

const AuthContext = createContext<AuthContextValue | null>(null);

const TOKEN_KEY = "rdc.auth.token";
const USER_KEY = "rdc.auth.user";
const LOCKED_KEY = "rdc.auth.locked";
const LOCKED_REASON_KEY = "rdc.auth.locked.reason";

export function AuthProvider({ children }: PropsWithChildren) {
  const [token, setToken] = useState<string | null>(() => localStorage.getItem(TOKEN_KEY));
  const [user, setUser] = useState<AuthUser | null>(() => {
    const raw = localStorage.getItem(USER_KEY);
    if (!raw) return null;
    try {
      return JSON.parse(raw) as AuthUser;
    } catch {
      return null;
    }
  });
  const [loading, setLoading] = useState(false);
  const [locked, setLocked] = useState<boolean>(() => localStorage.getItem(LOCKED_KEY) === "true");
  const [lockedReason, setLockedReason] = useState<string | undefined>(() => {
    const raw = localStorage.getItem(LOCKED_REASON_KEY);
    return raw || undefined;
  });

  const clearLock = useCallback(() => {
    setLocked(false);
    setLockedReason(undefined);
    localStorage.removeItem(LOCKED_KEY);
    localStorage.removeItem(LOCKED_REASON_KEY);
  }, []);

  // Local (client-side) auth mode:
  // - We trust the persisted token/user pair if present.
  // - No server-side verification is performed here.
  // This keeps the app runnable without requiring /api/auth endpoints.
  // If you later add a real auth service, you can re-introduce token verification.

  const setSession = useCallback((nextToken: string, nextUser: AuthUser) => {
    setToken(nextToken);
    setUser(nextUser);
    localStorage.setItem(TOKEN_KEY, nextToken);
    localStorage.setItem(USER_KEY, JSON.stringify(nextUser));
    localStorage.setItem("rdc.lastUsername", nextUser.username);
    clearLock();
  }, [clearLock]);

  const login = useCallback(
    async (username: string, password: string) => {
      setLoading(true);
      try {
        const result = verifyOrRegisterUser(username, password);
        if (!result.ok) throw new Error(result.error);
        const nextUser: AuthUser = { username: username.trim(), role: "admin" };
        // Dummy token to satisfy app logic that expects a token string.
        const nextToken = `local-${Date.now()}-${Math.random().toString(16).slice(2)}`;
        setSession(nextToken, nextUser);
        localStorage.setItem("rdc.lastLoginAt", new Date().toISOString());
        localStorage.removeItem("rdc.lastLogoutAt");
      } finally {
        setLoading(false);
      }
    },
    [setSession]
  );

  const logout = useCallback(() => {
    setToken(null);
    setUser(null);
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(USER_KEY);
    localStorage.setItem("rdc.lastLogoutAt", new Date().toISOString());
  }, []);

  const lockSession = useCallback(
    (reason?: string) => {
      setLocked(true);
      setLockedReason(reason || "Session locked");
      localStorage.setItem(LOCKED_KEY, "true");
      if (reason) localStorage.setItem(LOCKED_REASON_KEY, reason);
      logout();
    },
    [logout]
  );

  const value = useMemo(
    () => ({
      user,
      token,
      loading,
      locked,
      lockedReason,
      login,
      logout,
      clearLock,
      lockSession,
      setSession,
    }),
    [user, token, loading, locked, lockedReason, login, logout, clearLock, lockSession, setSession]
  );

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error("AuthProvider missing");
  return ctx;
}