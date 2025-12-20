import { useEffect, useMemo, useState, type ReactNode } from "react";
import {
  ShieldCheck,
  User,
  ArrowRight,
  Globe,
  Activity,
  CheckCircle2,
  Sparkles,
  Fingerprint,
  Lock,
  Loader2,
  KeyRound,
  CheckCircle,
  AlertTriangle,
} from "lucide-react";
import { precheckUser, registerUser, setPassword as apiSetPassword } from "./api";
import { useAuth } from "./auth";

type Step = "username" | "login" | "setPassword" | "notFound";

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

type Props = {
  appName: string;
  lockedReason?: string;
};

export default function LoginPage({ appName, lockedReason }: Props) {
  const { login, loading: authLoading, clearLock } = useAuth();

  const [username, setUsername] = useState(() => localStorage.getItem("rdc.lastUsername") || "");
  const [password, setPassword] = useState("");
  const [newPassword, setNewPassword] = useState("");
  const [confirmPassword, setConfirmPassword] = useState("");

  const [step, setStep] = useState<Step>("username");
  const [status, setStatus] = useState<{ exists: boolean; hasPassword: boolean } | null>(null);
  const [checking, setChecking] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [message, setMessage] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [prev, setPrev] = useState<PrevSession | null>(null);
  const passwordDisabled = true;

  useEffect(() => {
    const lastUsername = localStorage.getItem("rdc.lastUsername") || "";
    const lastLoginAt = localStorage.getItem("rdc.lastLoginAt");
    const lastLogoutAt = localStorage.getItem("rdc.lastLogoutAt");
    if (lastUsername) setPrev({ username: lastUsername, loginAt: lastLoginAt, logoutAt: lastLogoutAt });
  }, []);

  const usernameTrimmed = useMemo(() => username.trim(), [username]);
  const canCheck = usernameTrimmed.length >= 2;
  const canLogin = !passwordDisabled && usernameTrimmed.length >= 2 && password.trim().length >= 4 && !submitting && !authLoading;
  const canSetPassword =
    usernameTrimmed.length >= 2 &&
    newPassword.trim().length >= 8 &&
    newPassword === confirmPassword &&
    !submitting;

  const duration = useMemo(
    () => calcDuration(prev?.loginAt ?? null, prev?.logoutAt ?? null),
    [prev?.loginAt, prev?.logoutAt]
  );

  const handleCheckStatus = async () => {
    if (!canCheck) {
      setError("Please enter a username.");
      return;
    }
    setError(null);
    setMessage(null);
    setChecking(true);
    try {
      const res = await precheckUser(usernameTrimmed);
      setStatus(res);
      if (!res.exists) setStep("notFound");
      else if (!res.hasPassword) setStep("setPassword");
      else setStep("login");
    } catch (e: any) {
      setError(e?.message || "Unable to check account status.");
      // Allow manual login even if the precheck endpoint is unavailable.
      setStep("login");
      setStatus({ exists: true, hasPassword: true });
    } finally {
      setChecking(false);
    }
  };

  const handleLogin = async (e?: React.FormEvent) => {
    e?.preventDefault();
    if (passwordDisabled) {
      setError(null);
      setMessage("Login is temporarily disabled while the authentication UI is being revised.");
      return;
    }
    if (!canLogin) return;
    setSubmitting(true);
    setError(null);
    setMessage(null);
    try {
      await login(usernameTrimmed, passwordDisabled ? "" : password);
      clearLock();
    } catch (e: any) {
      setError(e?.message || "Login failed");
    } finally {
      setSubmitting(false);
    }
  };

  const handleSetPassword = async (e?: React.FormEvent) => {
    e?.preventDefault();
    if (!canSetPassword) return;
    setSubmitting(true);
    setError(null);
    setMessage(null);
    try {
      await apiSetPassword({
        username: usernameTrimmed,
        password: newPassword.trim(),
      });
      setMessage("Password set. Please log in to continue.");
      setStep("login");
      setStatus((prevStatus) => prevStatus ? { ...prevStatus, hasPassword: true, exists: true } : { exists: true, hasPassword: true });
      setPassword(newPassword);
      setNewPassword("");
      setConfirmPassword("");
    } catch (e: any) {
      setError(e?.message || "Failed to set password.");
    } finally {
      setSubmitting(false);
    }
  };

  const handleRegister = async () => {
    if (!canCheck) return;
    setSubmitting(true);
    setError(null);
    try {
      const res = await registerUser({ username: usernameTrimmed });
      if (!res.ok) {
        setError(res.error || "Unable to register user.");
      } else {
        setStatus({ exists: true, hasPassword: false });
        setStep("setPassword");
        setMessage("Account created. Set your password to continue.");
      }
    } catch (e: any) {
      setError(e?.message || "Unable to register.");
    } finally {
      setSubmitting(false);
    }
  };

  const showLoginForm = step === "login";
  const showSetPassword = step === "setPassword";
  const showNotFound = step === "notFound";

  return (
    <div className="min-h-screen bg-[#040A18] text-slate-100 relative overflow-hidden">
      <div className="pointer-events-none absolute inset-0">
        <div className="absolute -top-56 -left-40 h-[680px] w-[680px] rounded-full bg-blue-600/18 blur-3xl" />
        <div className="absolute -bottom-56 -right-48 h-[720px] w-[720px] rounded-full bg-cyan-500/14 blur-3xl" />
        <div className="absolute inset-0 bg-[radial-gradient(circle_at_18%_12%,rgba(59,130,246,0.22),transparent_45%),radial-gradient(circle_at_82%_74%,rgba(34,211,238,0.16),transparent_48%)]" />
        <div className="absolute inset-0 opacity-[0.08] [background-image:linear-gradient(to_right,rgba(148,163,184,0.22)_1px,transparent_1px),linear-gradient(to_bottom,rgba(148,163,184,0.22)_1px,transparent_1px)] [background-size:46px_46px]" />
      </div>

      <div className="relative mx-auto max-w-6xl px-6 py-10 min-h-screen flex items-center">
        <div className="w-full grid grid-cols-1 lg:grid-cols-2 gap-8 items-stretch">
          <div className="relative rounded-3xl border border-slate-700/40 bg-slate-900/25 backdrop-blur-xl p-8 lg:p-10 shadow-[0_30px_120px_-35px_rgba(37,99,235,0.45)] overflow-hidden">
            <div className="pointer-events-none absolute -top-24 -left-24 h-56 w-56 rounded-full bg-white/5 blur-2xl" />
            <div className="pointer-events-none absolute -bottom-24 -right-24 h-56 w-56 rounded-full bg-white/5 blur-2xl" />

            <div className="flex items-center gap-3">
              <div className="h-12 w-12 rounded-2xl bg-blue-500/15 border border-blue-400/30 flex items-center justify-center">
                <ShieldCheck className="h-6 w-6 text-blue-300" />
              </div>
              <div className="min-w-0">
                <p className="text-xs uppercase tracking-[0.35em] text-slate-400">Operator Console</p>
                <h1 className="text-2xl sm:text-3xl font-semibold tracking-tight text-slate-50 truncate">{appName}</h1>
              </div>
            </div>

            <div className="mt-6 inline-flex items-center gap-2 rounded-full border border-slate-700/50 bg-slate-950/25 px-3 py-1.5 text-[11px] text-slate-200">
              <Sparkles className="h-4 w-4 text-slate-200" />
              Session entry gateway
            </div>

            <p className="mt-5 text-sm leading-relaxed text-slate-300 max-w-prose">
              Sign in to start an operator session. The console requires a verified account and a valid token before any
              remote tasks can run.
            </p>

            <div className="mt-6 h-px w-full bg-gradient-to-r from-transparent via-slate-600/40 to-transparent" />

            <div className="mt-7 grid grid-cols-1 sm:grid-cols-2 gap-3">
              <Feature icon={<Globe className="h-4 w-4 text-cyan-300" />} title="LAN-ready" desc="Discovery uses broadcast only—no port scans." />
              <Feature icon={<Activity className="h-4 w-4 text-blue-300" />} title="Operational visibility" desc="Processes, console output, and session logs in one place." />
              <Feature icon={<CheckCircle2 className="h-4 w-4 text-emerald-300" />} title="Structured workflow" desc="Targets, actions, and commands designed for speed." />
              <Feature icon={<Fingerprint className="h-4 w-4 text-indigo-300" />} title="Session-first" desc="Clear lifecycle: enter, operate, lock or sign out." />
            </div>

            {prev ? (
              <div className="mt-8 rounded-2xl border border-slate-700/60 bg-slate-900/50 p-4">
                <div className="flex items-center gap-2 text-sm text-slate-200">
                  <HistoryBadge /> Previous session
                </div>
                <div className="mt-3 grid grid-cols-2 gap-3 text-sm text-slate-300">
                  <div>
                    <p className="text-xs text-slate-400">User</p>
                    <p className="font-medium text-slate-100">{prev.username}</p>
                  </div>
                  <div>
                    <p className="text-xs text-slate-400">Duration</p>
                    <p className="font-medium text-slate-100">{duration || "—"}</p>
                  </div>
                  <div>
                    <p className="text-xs text-slate-400">Login</p>
                    <p className="font-medium text-slate-100">{formatDate(prev.loginAt)}</p>
                  </div>
                  <div>
                    <p className="text-xs text-slate-400">Logout</p>
                    <p className="font-medium text-slate-100">{formatDate(prev.logoutAt)}</p>
                  </div>
                </div>
              </div>
            ) : null}
          </div>

          <div className="relative rounded-3xl border border-slate-700/40 bg-slate-900/40 backdrop-blur-xl p-6 sm:p-8 shadow-[0_30px_120px_-35px_rgba(59,130,246,0.45)]">
            {lockedReason ? (
              <div className="mb-4 flex items-center gap-2 rounded-xl border border-amber-400/30 bg-amber-500/10 px-3 py-2 text-sm text-amber-200">
                <Lock className="h-4 w-4" />
                <span>{lockedReason}</span>
              </div>
            ) : null}

            <div className="flex items-center gap-2 text-xs uppercase tracking-[0.32em] text-slate-400">
              <User className="h-4 w-4" />
              Operator access
            </div>
            <h2 className="mt-2 text-2xl font-semibold text-slate-50">Authenticate to continue</h2>
            <p className="text-sm text-slate-400">Status check → set password (first time) → login.</p>

            <form className="mt-6 space-y-4" onSubmit={showLoginForm ? handleLogin : showSetPassword ? handleSetPassword : handleCheckStatus}>
              <label className="block space-y-1.5">
                <div className="flex items-center justify-between text-sm text-slate-300">
                  <span>Username</span>
                  {status ? (
                    <span className="text-xs text-slate-400">
                      {status.exists ? (status.hasPassword ? "Ready" : "Password not set") : "Not found"}
                    </span>
                  ) : null}
                </div>
                <div className="relative">
                  <input
                    value={username}
                    onChange={(e) => {
                      setUsername(e.target.value);
                      setStatus(null);
                      setStep("username");
                    }}
                    onBlur={handleCheckStatus}
                    placeholder="Your operator username"
                    className="w-full rounded-xl border border-slate-700 bg-slate-950/50 px-3 py-2.5 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/60"
                  />
                  <button
                    type="button"
                    onClick={handleCheckStatus}
                    disabled={!canCheck || checking}
                    className="absolute right-1.5 top-1.5 inline-flex items-center gap-1 rounded-lg bg-slate-800 px-3 py-1.5 text-xs text-slate-100 hover:bg-slate-700 disabled:cursor-not-allowed disabled:opacity-60"
                  >
                    {checking ? <Loader2 className="h-4 w-4 animate-spin" /> : <ArrowRight className="h-4 w-4" />} Check
                  </button>
                </div>
              </label>

              {showSetPassword ? (
                <div className="space-y-3 rounded-xl border border-blue-500/30 bg-blue-500/5 p-4">
                  <div className="flex items-center gap-2 text-sm font-medium text-blue-100">
                    <KeyRound className="h-4 w-4" />
                    First-time setup
                  </div>
                  <p className="text-xs text-blue-100/80">
                    Choose a strong password to activate your operator account.
                  </p>
                  <input
                    type="password"
                    value={newPassword}
                    onChange={(e) => setNewPassword(e.target.value)}
                    placeholder="New password (min 8 chars)"
                    className="w-full rounded-lg border border-slate-700 bg-slate-950/60 px-3 py-2 text-sm text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/60"
                  />
                  <input
                    type="password"
                    value={confirmPassword}
                    onChange={(e) => setConfirmPassword(e.target.value)}
                    placeholder="Confirm password"
                    className="w-full rounded-lg border border-slate-700 bg-slate-950/60 px-3 py-2 text-sm text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/60"
                  />
                  <button
                    type="submit"
                    disabled={!canSetPassword}
                    className="w-full inline-flex items-center justify-center gap-2 rounded-lg bg-blue-500 px-3 py-2 text-sm font-semibold text-slate-50 hover:bg-blue-600 transition disabled:cursor-not-allowed disabled:opacity-60"
                  >
                    {submitting ? <Loader2 className="h-4 w-4 animate-spin" /> : <CheckCircle className="h-4 w-4" />}
                    Set password
                  </button>
                </div>
              ) : null}

              {showLoginForm ? (
                <div className="space-y-3">
                  <label className="block space-y-1.5">
                    <div className="flex items-center justify-between text-sm text-slate-300">
                      <span>Password</span>
                    </div>
                    <input
                      type="password"
                      value={password}
                      onChange={(e) => setPassword(e.target.value)}
                      placeholder={passwordDisabled ? "Password entry disabled for UI review" : "Enter your password"}
                      disabled={passwordDisabled}
                      className="w-full rounded-xl border border-slate-700 bg-slate-950/60 px-3 py-2.5 text-slate-100 focus:outline-none focus:ring-2 focus:ring-primary/60 disabled:cursor-not-allowed disabled:opacity-60"
                    />
                  </label>
                  {passwordDisabled ? (
                    <div className="rounded-lg border border-amber-500/40 bg-amber-500/10 px-3 py-2 text-xs text-amber-100">
                      Login is disabled while the authentication UI is being revised.
                    </div>
                  ) : null}
                  <button
                    type="submit"
                    disabled={!canLogin}
                    className="w-full inline-flex items-center justify-center gap-2 rounded-lg bg-primary px-3 py-2.5 text-sm font-semibold text-slate-50 hover:opacity-90 transition disabled:cursor-not-allowed disabled:opacity-60"
                  >
                    {submitting || authLoading ? <Loader2 className="h-4 w-4 animate-spin" /> : <ArrowRight className="h-4 w-4" />}
                    Sign in
                  </button>
                </div>
              ) : null}

              {showNotFound ? (
                <div className="space-y-3">
                  <div className="rounded-xl border border-amber-500/40 bg-amber-500/10 px-3 py-3 text-sm text-amber-100 flex gap-2 items-center">
                    <AlertTriangle className="h-4 w-4" />
                    Account not found. You can request a new operator account.
                  </div>
                  <button
                    type="button"
                    disabled={!canCheck || submitting}
                    onClick={handleRegister}
                    className="w-full inline-flex items-center justify-center gap-2 rounded-lg bg-slate-800 px-3 py-2 text-sm font-semibold text-slate-100 hover:bg-slate-700 disabled:opacity-60"
                  >
                    {submitting ? <Loader2 className="h-4 w-4 animate-spin" /> : <CheckCircle className="h-4 w-4" />}
                    Create account
                  </button>
                </div>
              ) : null}

              {error ? (
                <div className="rounded-xl border border-rose-500/40 bg-rose-500/10 px-3 py-3 text-sm text-rose-100 flex gap-2 items-center">
                  <AlertTriangle className="h-4 w-4" />
                  {error}
                </div>
              ) : null}

              {message ? (
                <div className="rounded-xl border border-emerald-500/40 bg-emerald-500/10 px-3 py-3 text-sm text-emerald-100 flex gap-2 items-center">
                  <CheckCircle className="h-4 w-4" />
                  {message}
                </div>
              ) : null}
            </form>

            <div className="mt-6 grid grid-cols-2 gap-3 text-xs text-slate-400">
              <div className="rounded-lg border border-slate-700 bg-slate-950/40 px-3 py-2">
                <p className="font-semibold text-slate-100">Flow</p>
                <p>1) Check status</p>
                <p>2) Set password (if needed)</p>
                <p>3) Login</p>
              </div>
              <div className="rounded-lg border border-slate-700 bg-slate-950/40 px-3 py-2">
                <p className="font-semibold text-slate-100">Security</p>
                <p>Tokens stored locally for the session.</p>
                <p>Lock anytime from the console.</p>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function Feature({ icon, title, desc }: { icon: ReactNode; title: string; desc: string }) {
  return (
    <div className="rounded-xl border border-slate-700/60 bg-slate-950/30 p-3.5">
      <div className="flex items-center gap-2 text-sm font-semibold text-slate-100">
        {icon}
        <span>{title}</span>
      </div>
      <p className="mt-1 text-xs text-slate-400 leading-relaxed">{desc}</p>
    </div>
  );
}

function HistoryBadge() {
  return (
    <span className="inline-flex items-center gap-1 rounded-full border border-slate-700 bg-slate-800/80 px-2 py-1 text-[11px] text-slate-200">
      <Lock className="h-3 w-3" /> Session log
    </span>
  );
}
