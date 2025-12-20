import { Wifi, Terminal } from "lucide-react";

interface HeaderProps {
  title?: string;
  connectedCount?: number;
}

export default function Header({
  connectedCount = 0,
}: HeaderProps) {
  return (
    <header
      className="
        flex items-center justify-between px-6 py-4
        border-b border-slate-700/40
        bg-slate-900/40 backdrop-blur-xl
      "
    >
      {/* LEFT */}
      <div className="flex items-center gap-4">
        <div
          className="
            w-10 h-10 rounded-xl
            bg-blue-500/15 border border-blue-400/30
            flex items-center justify-center
          "
        >
          <Terminal className="w-5 h-5 text-blue-300" />
        </div>

        <div className="flex flex-col">
          <span className="text-xs tracking-widest text-slate-400">
            NETWORK ADMINISTRATION & SECURITY TOOL
          </span>
          <h1 className="text-2xl font-semibold text-slate-50">
            Remote Control Center
          </h1>
        </div>
      </div>

      {/* RIGHT */}
      <div
        className="
          flex items-center gap-3 px-4 py-2
          rounded-full border border-slate-700/50
          bg-slate-900/50 text-sm
        "
      >
        <Wifi className="w-4 h-4 text-cyan-300" />
        <span className="text-slate-400">Connected:</span>
        <span className="font-semibold text-slate-100">
          {connectedCount}
        </span>
      </div>
    </header>
  );
}
