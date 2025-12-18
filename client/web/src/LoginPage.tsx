import { useEffect, useMemo, useState } from "react";
import type { ReactNode } from "react";
import {
  ShieldCheck,
  User,
  ArrowRight,
  Globe,
  Activity,
  CheckCircle2,
  AlertTriangle,
  History,
  LogIn,
  Sparkles,
  Fingerprint,
  Lock,
} from "lucide-react";

type Props = {
  appName: string;
  username: string;
  password: string; // [ADDED]
  onUsernameChange: (v: string) => void;
  onPasswordChange: (v: string) => void; // [ADDED]
  onSubmit: () => void;

  loading?: boolean;
  error?: string | null;
};

type PrevSession = {
  username: string;
  loginAt?: string | null;
  logoutAt?: string | null;
};

function formatDate(iso?: string | null) {
  if (!iso) return "—";
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return "—";
  return d.toLocaleString();
}

function calcDuration(loginAt?: string | null, logoutAt?: string | null) {
  if (!loginAt || !logoutAt) return null;
  const a = new Date(loginAt).getTime();
  const b = new Date(logoutAt).getTime();
  if (!Number.isFinite(a) || !Number.isFinite(b) || b < a) return null;
  const sec = Math.floor((b - a) / 1000);
  const m = Math.floor(sec / 60);
  const s = sec % 60;
  if (m < 60) return `${m}m ${s}s`;
  const h = Math.floor(m / 60);
  const mm = m % 60;
  return `${h}h ${mm}m`;
}

export default function LoginPage({
  appName,
  username,
  password, // [ADDED]
  onUsernameChange,
  onPasswordChange, // [ADDED]
  onSubmit,
  loading = false,
  error = null,
}: Props) {
  const [touched, setTouched] = useState(false);
  const [prev, setPrev] = useState<PrevSession | null>(null);

  useEffect(() => {
    const lastUsername = localStorage.getItem("rdc.lastUsername") || "";
    const lastLoginAt = localStorage.getItem("rdc.lastLoginAt");
    const lastLogoutAt = localStorage.getItem("rdc.lastLogoutAt");

    if (lastUsername) {
      setPrev({
        username: lastUsername,
        loginAt: lastLoginAt,
        logoutAt: lastLogoutAt,
      });
    } else {
      setPrev(null);
    }
  }, []);

  const usernameTrimmed = useMemo(() => username.trim(), [username]);
  const passwordTrimmed = useMemo(() => password.trim(), [password]); // [ADDED]

  // [CHANGED] tách error theo field cho đúng UX
  const usernameError = useMemo(() => {
    if (!touched) return null;
    if (!usernameTrimmed) return "Username is required.";
    if (usernameTrimmed.length < 2) return "Username must be at least 2 characters.";
    return null;
  }, [touched, usernameTrimmed]);

  const passwordError = useMemo(() => {
    if (!touched) return null;
    if (!passwordTrimmed) return "Password is required.";
    if (passwordTrimmed.length < 4) return "Password must be at least 4 characters.";
    return null;
  }, [touched, passwordTrimmed]);

  const localError = useMemo(() => {
    // Ưu tiên hiện lỗi username trước (như flow nhập liệu)
    return usernameError || passwordError;
  }, [usernameError, passwordError]);

  const canSubmit = useMemo(() => {
    return !!usernameTrimmed && !!passwordTrimmed && !loading;
  }, [usernameTrimmed, passwordTrimmed, loading]);

  // [CHANGED] submit chuẩn theo form
  const handleSubmit = (e?: React.FormEvent) => {
    e?.preventDefault();
    setTouched(true);
    if (loading) return;
    if (!usernameTrimmed) return;
    if (!passwordTrimmed) return;
    onSubmit();
  };

  const duration = useMemo(
    () => calcDuration(prev?.loginAt ?? null, prev?.logoutAt ?? null),
    [prev?.loginAt, prev?.logoutAt]
  );

  return (
    <div className="min-h-screen bg-[#040A18] text-slate-100 relative overflow-hidden">
      {/* Background */}
      <div className="pointer-events-none absolute inset-0">
        <div className="absolute -top-56 -left-40 h-[680px] w-[680px] rounded-full bg-blue-600/18 blur-3xl" />
        <div className="absolute -bottom-56 -right-48 h-[720px] w-[720px] rounded-full bg-cyan-500/14 blur-3xl" />
        <div className="absolute inset-0 bg-[radial-gradient(circle_at_18%_12%,rgba(59,130,246,0.22),transparent_45%),radial-gradient(circle_at_82%_74%,rgba(34,211,238,0.16),transparent_48%)]" />
        <div className="absolute inset-0 opacity-[0.08] [background-image:linear-gradient(to_right,rgba(148,163,184,0.22)_1px,transparent_1px),linear-gradient(to_bottom,rgba(148,163,184,0.22)_1px,transparent_1px)] [background-size:46px_46px]" />

        <div className="absolute left-[10%] top-[18%] h-2 w-2 rounded-full bg-white/20 blur-[1px]" />
        <div className="absolute left-[18%] top-[28%] h-1.5 w-1.5 rounded-full bg-white/15 blur-[1px]" />
        <div className="absolute right-[14%] top-[22%] h-2 w-2 rounded-full bg-white/20 blur-[1px]" />
        <div className="absolute right-[20%] top-[34%] h-1.5 w-1.5 rounded-full bg-white/15 blur-[1px]" />
      </div>

      <div className="relative mx-auto max-w-6xl px-6 py-10 min-h-screen flex items-center">
        <div className="w-full grid grid-cols-1 lg:grid-cols-2 gap-8 items-stretch">
          {/* Left: Brand */}
          <div className="relative rounded-3xl border border-slate-700/40 bg-slate-900/25 backdrop-blur-xl p-8 lg:p-10 shadow-[0_30px_120px_-35px_rgba(37,99,235,0.45)] overflow-hidden">
            {/* Decorative top highlight */}
            <div className="pointer-events-none absolute -top-24 -left-24 h-56 w-56 rounded-full bg-white/5 blur-2xl" />
            <div className="pointer-events-none absolute -bottom-24 -right-24 h-56 w-56 rounded-full bg-white/5 blur-2xl" />

            <div className="flex items-center gap-3">
              <div className="h-12 w-12 rounded-2xl bg-blue-500/15 border border-blue-400/30 flex items-center justify-center">
                <ShieldCheck className="h-6 w-6 text-blue-300" />
              </div>
              <div className="min-w-0">
                <p className="text-xs uppercase tracking-[0.35em] text-slate-400">
                  Operator Console
                </p>
                <h1 className="text-2xl sm:text-3xl font-semibold tracking-tight text-slate-50 truncate">
                  {appName}
                </h1>
              </div>
            </div>

            <div className="mt-6 inline-flex items-center gap-2 rounded-full border border-slate-700/50 bg-slate-950/25 px-3 py-1.5 text-[11px] text-slate-200">
              <Sparkles className="h-4 w-4 text-slate-200" />
              Session entry gateway
            </div>

            <p className="mt-5 text-sm leading-relaxed text-slate-300 max-w-prose">
              Sign in to start an operator session.
            </p>

            <div className="mt-6 h-px w-full bg-gradient-to-r from-transparent via-slate-600/40 to-transparent" />

            <div className="mt-7 grid grid-cols-1 sm:grid-cols-2 gap-3">
              <Feature
                icon={<Globe className="h-4 w-4 text-cyan-300" />}
                title="LAN-ready"
                desc="Optimized for lab and internal network operations."
              />
              <Feature
                icon={<Activity className="h-4 w-4 text-blue-300" />}
                title="Operational visibility"
                desc="Processes, console output, and session logs in one place."
              />
              <Feature
                icon={<CheckCircle2 className="h-4 w-4 text-emerald-300" />}
                title="Structured workflow"
                desc="Targets, actions, and commands designed for speed."
              />
              <Feature
                icon={<Fingerprint className="h-4 w-4 text-indigo-300" />}
                title="Session-first"
                desc="Clear lifecycle: enter, operate, sign out."
              />
            </div>

            {/* Previous session */}
            {prev ? (
              <div className="mt-8 rounded-2xl border border-slate-700/50 bg-slate-950/25 p-4">
                <div className="flex items-center justify-between gap-3">
                  <div className="flex items-center gap-2">
                    <span className="inline-flex h-8 w-8 items-center justify-center rounded-xl bg-blue-500/10 border border-blue-400/20">
                      <History className="h-4 w-4 text-blue-300" />
                    </span>
                    <div>
                      <p className="text-xs font-semibold text-slate-100">
                        Previous session
                      </p>
                      <p className="text-[11px] text-slate-400">
                        Last user:{" "}
                        <span className="text-slate-200">{prev.username}</span>
                      </p>
                    </div>
                  </div>

                  <button
                    type="button"
                    onClick={() => {
                      onUsernameChange(prev.username);
                      onPasswordChange(""); // Clear password when using last username
                      setTouched(true);
                    }}
                    className="inline-flex items-center gap-2 rounded-xl border border-slate-700/60 bg-slate-950/35 px-3 py-2 text-[11px] text-slate-200 hover:border-blue-500/40 hover:bg-slate-950/55 transition"
                    title="Use last username"
                  >
                    <LogIn className="h-4 w-4 text-cyan-300" />
                    Use
                  </button>
                </div>

                <div className="mt-3 grid grid-cols-1 sm:grid-cols-3 gap-2 text-[11px] text-slate-400">
                  <div className="rounded-xl border border-slate-700/40 bg-slate-950/20 px-3 py-2">
                    <p className="text-slate-500">Login</p>
                    <p className="text-slate-200">{formatDate(prev.loginAt)}</p>
                  </div>
                  <div className="rounded-xl border border-slate-700/40 bg-slate-950/20 px-3 py-2">
                    <p className="text-slate-500">Logout</p>
                    <p className="text-slate-200">{formatDate(prev.logoutAt)}</p>
                  </div>
                  <div className="rounded-xl border border-slate-700/40 bg-slate-950/20 px-3 py-2">
                    <p className="text-slate-500">Duration</p>
                    <p className="text-slate-200">{duration ?? "—"}</p>
                  </div>
                </div>
              </div>
            ) : (
              <div className="mt-8 rounded-2xl border border-blue-500/20 bg-blue-500/10 p-4">
                <p className="text-xs text-slate-200">
                  No previous session recorded yet. Your last login will appear
                  here after you sign out.
                </p>
              </div>
            )}
          </div>

          {/* Right: Login */}
          <div className="relative rounded-3xl border border-slate-700/40 bg-slate-900/30 backdrop-blur-xl shadow-2xl p-8 lg:p-10 flex flex-col justify-center overflow-hidden">
            <div className="pointer-events-none absolute -top-24 -right-24 h-64 w-64 rounded-full bg-blue-500/10 blur-3xl" />
            <div className="pointer-events-none absolute -bottom-24 -left-24 h-64 w-64 rounded-full bg-cyan-500/10 blur-3xl" />

            <div className="absolute left-8 right-8 top-6 h-px bg-gradient-to-r from-transparent via-slate-600/50 to-transparent" />

            <div className="flex items-start justify-between gap-4">
              <div>
                <p className="text-3xl sm:text-4xl font-semibold tracking-tight text-slate-50 leading-tight">
                  Sign in
                </p>
                <p className="text-sm text-slate-400 mt-2">
                  Enter your operator credentials to continue.
                </p>
              </div>

              <div className="h-12 w-12 rounded-2xl bg-blue-500/10 border border-blue-400/20 flex items-center justify-center">
                <User className="h-6 w-6 text-blue-300" />
              </div>
            </div>

            {/* [CHANGED] dùng form để submit chuẩn */}
            <form className="mt-8 space-y-3" onSubmit={handleSubmit}>
              <label className="text-sm font-semibold text-slate-200">
                Username
              </label>

              <div
                className={[
                  "flex items-center gap-3 rounded-2xl border bg-slate-950/40 px-4 py-4 transition",
                  "focus-within:ring-2 focus-within:ring-blue-500/25",
                  usernameError
                    ? "border-rose-500/50"
                    : "border-slate-700/60 focus-within:border-blue-500/70",
                ].join(" ")}
              >
                <User className="h-5 w-5 text-slate-400" />
                <input
                  value={username}
                  onChange={(e) => onUsernameChange(e.target.value)}
                  onBlur={() => setTouched(true)}
                  placeholder="e.g. admin"
                  className="w-full bg-transparent outline-none text-base text-slate-100 placeholder:text-slate-500"
                  autoFocus
                  aria-invalid={!!usernameError}
                />
                {usernameTrimmed ? (
                  <span className="inline-flex items-center gap-1 text-[12px] text-emerald-300">
                    <CheckCircle2 className="h-5 w-5" />
                  </span>
                ) : null}
              </div>

              <label className="text-sm font-semibold text-slate-200 mt-2 block">
                Password
              </label>

              <div
                className={[
                  "flex items-center gap-3 rounded-2xl border bg-slate-950/40 px-4 py-4 transition",
                  "focus-within:ring-2 focus-within:ring-blue-500/25",
                  passwordError
                    ? "border-rose-500/50"
                    : "border-slate-700/60 focus-within:border-blue-500/70",
                ].join(" ")}
              >
                <Lock className="h-5 w-5 text-slate-400" />
                <input
                  value={password}
                  onChange={(e) => onPasswordChange(e.target.value)}
                  onBlur={() => setTouched(true)}
                  placeholder="Enter password"
                  type="password"
                  className="w-full bg-transparent outline-none text-base text-slate-100 placeholder:text-slate-500"
                  aria-invalid={!!passwordError}
                />
                {passwordTrimmed ? (
                  <span className="inline-flex items-center gap-1 text-[12px] text-emerald-300">
                    <CheckCircle2 className="h-5 w-5" />
                  </span>
                ) : null}
              </div>

              {localError ? <Banner tone="danger" text={localError} /> : null}
              {error ? <Banner tone="warn" text={error} /> : null}

              <button
                type="submit"
                disabled={!canSubmit}
                className="mt-4 inline-flex w-full items-center justify-center gap-2 rounded-2xl bg-gradient-to-r from-blue-600 to-cyan-500 px-5 py-4 text-base font-semibold text-white shadow-lg shadow-blue-600/20 transition hover:from-blue-500 hover:to-cyan-400 disabled:opacity-50 disabled:cursor-not-allowed"
              >
                Continue
                <ArrowRight className="h-5 w-5" />
              </button>

              <p className="mt-4 text-[12px] text-slate-500 leading-relaxed">
                By continuing, you confirm you are authorized to access and
                control the target machines.
              </p>
            </form>

            <div className="mt-8 flex items-center justify-between text-[11px] text-slate-500">
              <span>Session Entry</span>
              <span>© {appName}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function Feature({
  icon,
  title,
  desc,
}: {
  icon: ReactNode;
  title: string;
  desc: string;
}) {
  return (
    <div className="rounded-2xl border border-slate-700/40 bg-slate-950/25 p-4 hover:bg-slate-950/35 transition">
      <div className="flex items-center gap-2">
        <span className="inline-flex h-8 w-8 items-center justify-center rounded-xl bg-slate-900/60 border border-slate-700/40">
          {icon}
        </span>
        <p className="text-xs font-semibold text-slate-100">{title}</p>
      </div>
      <p className="mt-2 text-[11px] text-slate-400 leading-relaxed">{desc}</p>
    </div>
  );
}

function Banner({ tone, text }: { tone: "danger" | "warn"; text: string }) {
  const cls =
    tone === "danger"
      ? "border-rose-500/25 bg-rose-500/10 text-rose-200"
      : "border-amber-500/25 bg-amber-500/10 text-amber-200";
  return (
    <div className={`rounded-2xl border px-4 py-3 text-xs flex items-start gap-2 ${cls}`}>
      <AlertTriangle className="h-4 w-4 mt-0.5" />
      <span>{text}</span>
    </div>
  );
}
